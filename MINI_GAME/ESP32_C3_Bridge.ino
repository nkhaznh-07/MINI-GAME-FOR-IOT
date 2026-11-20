#include <Arduino.h>
#include <WiFi.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include "mbedtls/base64.h"

// THAY BẰNG ĐỊA CHỈ MAC THỰC TẾ CỦA MẠCH ESP32-S3 TAY CẦM
uint8_t s3PeerMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

#define WIFI_CHANNEL 1
#define MAX_PAYLOAD  180

// Struct gói dữ liệu C3 -> S3 (188 bytes < 210 bytes)
typedef struct __attribute__((packed)) {
  uint16_t magic;      // 0x5A77
  uint8_t  type;       // 1: Ảnh slide hiện tại, 2: Văn bản UTF-8, 3: Ping, 4: Ảnh slide kế tiếp
  uint16_t param1;     // Số thứ tự dòng ảnh (0..125) hoặc Offset byte chữ
  uint16_t param2;     // Offset byte trong dòng (0 hoặc 158) hoặc Tổng độ dài chữ
  uint8_t  len;        // Số byte thực tế trong data[]
  uint8_t  data[MAX_PAYLOAD];
} DataPacket;

// Struct gói lệnh S3 -> C3 (4 bytes)
typedef struct __attribute__((packed)) {
  uint16_t magic;      // 0x5A78
  uint8_t  cmd;        // 1: NEXT, 2: PREV
  uint8_t  checksum;
} CmdPacket;

String serialLine = "";
uint8_t decodeBuf[2048];

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
  if (len == sizeof(CmdPacket)) {
    CmdPacket pkt;
    memcpy(&pkt, incomingData, sizeof(CmdPacket));
    if (pkt.magic == 0x5A78 && pkt.checksum == (pkt.cmd ^ 0xAA)) {
      if (pkt.cmd == 1) Serial.println("CMD:NEXT");
      else if (pkt.cmd == 2) Serial.println("CMD:PREV");
    }
  }
}

void sendPacketSafe(DataPacket &pkt) {
  esp_now_send(s3PeerMac, (uint8_t*)&pkt, sizeof(DataPacket));
  delayMicroseconds(1150);
}

void forwardImageRow(uint8_t pktType, const String& line) {
  int secondColon = line.indexOf(':', 2);
  if (secondColon == -1) return;

  uint16_t rowIdx = (uint16_t)line.substring(2, secondColon).toInt();
  String b64 = line.substring(secondColon + 1);

  size_t outLen = 0;
  int ret = mbedtls_base64_decode(decodeBuf, sizeof(decodeBuf), &outLen,
                                  (const unsigned char*)b64.c_str(), b64.length());
  // Mỗi dòng ảnh 158px RGB565 = 316 bytes -> Chia làm 2 gói 158 bytes
  if (ret == 0 && outLen == 316) {
    for (uint16_t partOffset = 0; partOffset < 316; partOffset += 158) {
      DataPacket pkt = {};
      pkt.magic = 0x5A77;
      pkt.type = pktType;
      pkt.param1 = rowIdx;
      pkt.param2 = partOffset;
      pkt.len = 158;
      memcpy(pkt.data, decodeBuf + partOffset, 158);
      sendPacketSafe(pkt);
    }
  }
}

void processSerialLine(const String& line) {
  if (line.length() == 0) return;

  if (line == "P") {
    DataPacket pkt = {};
    pkt.magic = 0x5A77;
    pkt.type = 3;
    sendPacketSafe(pkt);
    return;
  }

  // Dòng ảnh slide hiện tại: I:<row_idx>:<base64_316bytes>
  if (line.startsWith("I:")) {
    forwardImageRow(1, line);
  }
  // Dòng ảnh slide kế tiếp: N:<row_idx>:<base64_316bytes>
  else if (line.startsWith("N:")) {
    forwardImageRow(4, line);
  }
  // Nội dung chữ: T:<base64_utf8>
  else if (line.startsWith("T:")) {
    String b64 = line.substring(2);
    if (b64.length() == 0) {
      const char* emptyMsg = "(Slide không có nội dung văn bản)";
      size_t msgLen = strlen(emptyMsg);
      DataPacket pkt = {};
      pkt.magic = 0x5A77;
      pkt.type = 2;
      pkt.param1 = 0;
      pkt.param2 = (uint16_t)msgLen;
      pkt.len = (uint8_t)msgLen;
      memcpy(pkt.data, emptyMsg, msgLen);
      sendPacketSafe(pkt);
      return;
    }

    size_t outLen = 0;
    int ret = mbedtls_base64_decode(decodeBuf, sizeof(decodeBuf), &outLen,
                                    (const unsigned char*)b64.c_str(), b64.length());
    if (ret == 0 && outLen > 0) {
      size_t offset = 0;
      while (offset < outLen) {
        size_t chunk = min((size_t)MAX_PAYLOAD, outLen - offset);
        DataPacket pkt = {};
        pkt.magic = 0x5A77;
        pkt.type = 2;
        pkt.param1 = (uint16_t)offset;
        pkt.param2 = (uint16_t)outLen;
        pkt.len = (uint8_t)chunk;
        memcpy(pkt.data, decodeBuf + offset, chunk);
        sendPacketSafe(pkt);
        offset += chunk;
      }
    }
  }
}

void setup() {
  Serial.begin(460800);
  Serial.setRxBufferSize(8192);
  serialLine.reserve(1024);

  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (esp_now_init() == ESP_OK) {
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, s3PeerMac, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);
  }
}

void loop() {
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == '\n') {
      serialLine.trim();
      processSerialLine(serialLine);
      serialLine = "";
    } else if (c != '\r') {
      serialLine += c;
    }
  }
}