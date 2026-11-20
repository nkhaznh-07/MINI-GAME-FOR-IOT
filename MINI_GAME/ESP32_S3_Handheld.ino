#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WiFiUdp.h>
#include <WebServer.h>
#include <ArduinoJson.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <vector>
#include <algorithm>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>

// =========================================================================
// 1. CẤU HÌNH THÔNG TIN CÁ NHÂN, WIFI NỘI BỘ & ESP-NOW
// =========================================================================
const char* PLAYER_INFO       = "Masv_NguyenDuyKhanh";
const char* DEFAULT_WIFI_SSID = "YOUR_WIFI_SSID";
const char* DEFAULT_WIFI_PASS = "YOUR_WIFI_PASS";
const char* AP_SSID           = "ESP32_GAME_2P";
const char* AP_PASS           = "12345678";

constexpr uint16_t LAN_UDP_PORT    = 4210;
constexpr size_t   MAX_TOP_PLAYERS = 10;

uint8_t c3PeerMac[]           = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define WIFI_CHANNEL          1
#define MAX_PAYLOAD           180

// =========================================================================
// 2. CHÂN PHẦN CỨNG
// =========================================================================
constexpr int TFT_CS_PIN   = 10;
constexpr int TFT_DC_PIN   = 9;
constexpr int TFT_RST_PIN  = 8;
constexpr int TFT_MOSI_PIN = 11;
constexpr int TFT_SCLK_PIN = 12;

constexpr int JOY_X_PIN    = 4;
constexpr int JOY_Y_PIN    = 5;
constexpr int JOY_SW_PIN   = 6;
constexpr int BUZZER_PIN   = 7;

constexpr int W = 128, H = 160;
constexpr uint32_t SPI_CLOCK = 40000000UL;
constexpr int SAFE_X = 2, SAFE_Y = 2, SAFE_W = 124, SAFE_H = 156;

// =========================================================================
// 3. CLASS BÀN PHÍM ẢO
// =========================================================================
enum KeyMode {
    MODE_LOWERCASE, MODE_UPPERCASE, MODE_NUMBERS, MODE_SYMBOLS
};

class VirtualKeyboard {
private:
    KeyMode currentMode;
    int screenWidth, screenHeight, kbY, kbHeight;
    int cursorRow, cursorCol;
    void (*drawKeyCallback)(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed);

    int colsInRow(int r) {
        if (r == 0 || r == 1) return 10;
        if (r == 2 || r == 3) return 9;
        return 5;
    }

public:
    VirtualKeyboard() : currentMode(MODE_LOWERCASE), cursorRow(0), cursorCol(0), drawKeyCallback(nullptr) {}

    void begin(int width, int height, int startY,
               void (*drawFunc)(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed)) {
        screenWidth = width;
        screenHeight = height;
        kbY = startY;
        kbHeight = height - startY;
        drawKeyCallback = drawFunc;
        cursorRow = 0;
        cursorCol = 0;
    }

    void resetCursor() { cursorRow = 0; cursorCol = 0; }
    void setMode(KeyMode mode) { currentMode = mode; }

    void moveCursor(int dx, int dy) {
        if (dy != 0) {
            cursorRow = (cursorRow + dy + 5) % 5;
            int maxCols = colsInRow(cursorRow);
            if (cursorCol >= maxCols) cursorCol = maxCols - 1;
        }
        if (dx != 0) {
            int maxCols = colsInRow(cursorRow);
            cursorCol = (cursorCol + dx + maxCols) % maxCols;
        }
    }

    void draw() {
        if (!drawKeyCallback) return;
        int rowHeight = kbHeight / 5;
        int margin = 1;
        int w10 = (screenWidth - (11 * margin)) / 10;

        const char* r1[] = {"1","2","3","4","5","6","7","8","9","0"};
        for (int i = 0; i < 10; i++) {
            drawKeyCallback(margin + i * (w10 + margin), kbY + margin, w10, rowHeight - margin * 2,
                            r1[i], false, (cursorRow == 0 && cursorCol == i));
        }

        const char* r2_l[] = {"q","w","e","r","t","y","u","i","o","p"};
        const char* r2_u[] = {"Q","W","E","R","T","Y","U","I","O","P"};
        const char* r2_n[] = {"[","]","{","}","#","%","^","*","+","="};
        const char* r2_s[] = {"_","\\","|","~","<",">","^","*","+","="};
        for (int i = 0; i < 10; i++) {
            const char* lbl = (currentMode == MODE_UPPERCASE) ? r2_u[i] :
                              (currentMode == MODE_NUMBERS)   ? r2_n[i] :
                              (currentMode == MODE_SYMBOLS)   ? r2_s[i] : r2_l[i];
            drawKeyCallback(margin + i * (w10 + margin), kbY + rowHeight + margin, w10, rowHeight - margin * 2,
                            lbl, false, (cursorRow == 1 && cursorCol == i));
        }

        const char* r3_l[] = {"a","s","d","f","g","h","j","k","l"};
        const char* r3_u[] = {"A","S","D","F","G","H","J","K","L"};
        const char* r3_n[] = {"-","/",":",";","(",")","$","&","@"};
        int offset9 = (screenWidth - (9 * w10 + 8 * margin)) / 2;
        for (int i = 0; i < 9; i++) {
            const char* lbl = (currentMode == MODE_UPPERCASE) ? r3_u[i] :
                              (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) ? r3_n[i] : r3_l[i];
            drawKeyCallback(offset9 + i * (w10 + margin), kbY + 2 * rowHeight + margin, w10, rowHeight - margin * 2,
                            lbl, false, (cursorRow == 2 && cursorCol == i));
        }

        int y4 = kbY + 3 * rowHeight + margin;
        int wSpecial = (screenWidth - (7 * w10 + 8 * margin)) / 2;
        const char* shiftLbl = (currentMode == MODE_NUMBERS) ? "#+=" : (currentMode == MODE_SYMBOLS) ? "123" : "^";
        drawKeyCallback(margin, y4, wSpecial, rowHeight - margin * 2, shiftLbl, true,
                        (cursorRow == 3 && cursorCol == 0) || (currentMode == MODE_UPPERCASE));

        const char* r4_l[] = {"z","x","c","v","b","n","m"};
        const char* r4_u[] = {"Z","X","C","V","B","N","M"};
        const char* r4_n[] = {".",",","?","!","'","\"","~"};
        for (int i = 0; i < 7; i++) {
            const char* lbl = (currentMode == MODE_UPPERCASE) ? r4_u[i] :
                              (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) ? r4_n[i] : r4_l[i];
            drawKeyCallback(margin + wSpecial + margin + i * (w10 + margin), y4, w10, rowHeight - margin * 2,
                            lbl, false, (cursorRow == 3 && cursorCol == i + 1));
        }
        drawKeyCallback(screenWidth - margin - wSpecial, y4, wSpecial, rowHeight - margin * 2,
                        "<X", true, (cursorRow == 3 && cursorCol == 8));

        int y5 = kbY + 4 * rowHeight + margin;
        int wMode  = (int)(w10 * 1.8f);
        int wEnter = (int)(w10 * 2.0f);
        int wComma = w10, wDot = w10;
        int wSpace = screenWidth - (wMode + wComma + wDot + wEnter + 6 * margin);
        const char* modeLbl = (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) ? "ABC" : "123";

        drawKeyCallback(margin, y5, wMode, rowHeight - margin * 2, modeLbl, true, (cursorRow == 4 && cursorCol == 0));
        drawKeyCallback(margin * 2 + wMode, y5, wComma, rowHeight - margin * 2, ",", false, (cursorRow == 4 && cursorCol == 1));
        drawKeyCallback(margin * 3 + wMode + wComma, y5, wSpace, rowHeight - margin * 2, "SPACE", false, (cursorRow == 4 && cursorCol == 2));
        drawKeyCallback(margin * 4 + wMode + wComma + wSpace, y5, wDot, rowHeight - margin * 2, ".", false, (cursorRow == 4 && cursorCol == 3));
        drawKeyCallback(margin * 5 + wMode + wComma + wSpace + wDot, y5, wEnter, rowHeight - margin * 2, "OK", true, (cursorRow == 4 && cursorCol == 4));
    }

    String pressSelectedKey() {
        if (cursorRow == 0) {
            const char* r1[] = {"1","2","3","4","5","6","7","8","9","0"};
            return r1[cursorCol];
        }
        if (cursorRow == 1) {
            const char* r2_l[] = {"q","w","e","r","t","y","u","i","o","p"};
            const char* r2_u[] = {"Q","W","E","R","T","Y","U","I","O","P"};
            const char* r2_n[] = {"[","]","{","}","#","%","^","*","+","="};
            const char* r2_s[] = {"_","\\","|","~","<",">","^","*","+","="};
            if (currentMode == MODE_UPPERCASE) return r2_u[cursorCol];
            if (currentMode == MODE_NUMBERS)   return r2_n[cursorCol];
            if (currentMode == MODE_SYMBOLS)   return r2_s[cursorCol];
            return r2_l[cursorCol];
        }
        if (cursorRow == 2) {
            const char* r3_l[] = {"a","s","d","f","g","h","j","k","l"};
            const char* r3_u[] = {"A","S","D","F","G","H","J","K","L"};
            const char* r3_n[] = {"-","/",":",";","(",")","$","&","@"};
            if (currentMode == MODE_UPPERCASE) return r3_u[cursorCol];
            if (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) return r3_n[cursorCol];
            return r3_l[cursorCol];
        }
        if (cursorRow == 3) {
            if (cursorCol == 0) {
                if (currentMode == MODE_LOWERCASE) currentMode = MODE_UPPERCASE;
                else if (currentMode == MODE_UPPERCASE) currentMode = MODE_LOWERCASE;
                else if (currentMode == MODE_NUMBERS) currentMode = MODE_SYMBOLS;
                else currentMode = MODE_NUMBERS;
                return "";
            }
            if (cursorCol == 8) return "\b";
            const char* r4_l[] = {"z","x","c","v","b","n","m"};
            const char* r4_u[] = {"Z","X","C","V","B","N","M"};
            const char* r4_n[] = {".",",","?","!","'","\"","~"};
            int idx = cursorCol - 1;
            if (currentMode == MODE_UPPERCASE) return r4_u[idx];
            if (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) return r4_n[idx];
            return r4_l[idx];
        }
        if (cursorRow == 4) {
            if (cursorCol == 0) {
                currentMode = (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) ? MODE_LOWERCASE : MODE_NUMBERS;
                return "";
            }
            if (cursorCol == 1) return ",";
            if (cursorCol == 2) return " ";
            if (cursorCol == 3) return ".";
            if (cursorCol == 4) return "\n";
        }
        return "";
    }
};

// =========================================================================
// 4. ĐỐI TƯỢNG ĐỒ HỌA, BỘ ĐỆM STREAM MÀN HÌNH & MẠNG NỘI BỘ
// =========================================================================
Adafruit_ST7735 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
GFXcanvas16 canvas(128, 160);
GFXcanvas16 kbCanvas(160, 128);
U8G2_FOR_ADAFRUIT_GFX u8g2;
VirtualKeyboard vkb;

WebServer server(80);
WiFiUDP lanUdp;
Preferences prefs;
SemaphoreHandle_t dataMutex;
SemaphoreHandle_t frameMutex;

uint16_t webMirrorBuf[128 * 160];

constexpr uint16_t BLACK   = ST77XX_BLACK,   WHITE   = ST77XX_WHITE, RED    = ST77XX_RED;
constexpr uint16_t GREEN   = ST77XX_GREEN,   BLUE    = ST77XX_BLUE,  YELLOW = ST77XX_YELLOW;
constexpr uint16_t CYAN    = ST77XX_CYAN,    MAGENTA = ST77XX_MAGENTA, ORANGE = 0xFD20;
constexpr uint16_t GRAY    = 0x8410,         NAVY    = 0x000F,       PINK   = 0xF81F;

enum State {
    MENU, SNAKE, TETRIS, PACMAN, BREAKOUT, SETTING, SOUND_MENU, LEADERBOARD, WIFI_SCAN, WIFI_PASS_INPUT, PPT_REMOTE, RESULT
};
State state = MENU, lastGame = MENU;
bool twoPlayerMode = false;

void flushCanvas() {
    uint16_t* buf = canvas.getBuffer();
    tft.drawRGBBitmap(0, 0, buf, 128, 160);
    if (frameMutex && xSemaphoreTake(frameMutex, 0) == pdTRUE) {
        memcpy(webMirrorBuf, buf, 128 * 160 * sizeof(uint16_t));
        xSemaphoreGive(frameMutex);
    }
}

// =========================================================================
// 5. QUẢN LÝ WIFI NỘI BỘ & BIỂU TƯỢNG SÓNG WIFI
// =========================================================================
String savedSsid = "";
String savedPass = "";
volatile bool pauseNetworkTask = false;

std::vector<String> scannedSsids;
std::vector<int> scannedRssi;
int wifiListIdx = 0;
String targetSsid = "";
String inputPassword = "";

void drawWifiSignalIcon(int x, int y) {
    bool connected = (WiFi.status() == WL_CONNECTED);
    int rssi = connected ? WiFi.RSSI() : -100;
    int bars = 0;
    uint16_t activeCol = RED;

    if (connected) {
        if (rssi >= -60)      { bars = 4; activeCol = GREEN; }
        else if (rssi >= -70) { bars = 3; activeCol = GREEN; }
        else if (rssi >= -80) { bars = 2; activeCol = YELLOW; }
        else                  { bars = 1; activeCol = ORANGE; }
    }

    for (int i = 0; i < 4; i++) {
        int barH = 3 + i * 2;
        int bx = x + i * 3;
        int by = y + (9 - barH);
        uint16_t c = (i < bars) ? activeCol : 0x3186;
        canvas.fillRect(bx, by, 2, barH, c);
    }

    if (!connected) {
        canvas.drawLine(x, y + 8, x + 10, y + 1, RED);
    }
}

void drawVirtualKeyCallback(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed) {
    uint16_t bg = isPressed ? YELLOW : (isSpecial ? NAVY : 0x2104);
    uint16_t fg = isPressed ? BLACK  : (isSpecial ? CYAN   : WHITE);
    uint16_t bd = isPressed ? RED    : GRAY;

    kbCanvas.fillRoundRect(x, y, w, h, 2, bg);
    kbCanvas.drawRoundRect(x, y, w, h, 2, bd);

    kbCanvas.setTextSize(1);
    kbCanvas.setTextColor(fg);
    int16_t x1, y1;
    uint16_t tw, th;
    kbCanvas.getTextBounds(label, 0, 0, &x1, &y1, &tw, &th);
    kbCanvas.setCursor(x + max(1, (w - (int)tw) / 2), y + max(1, (h - (int)th) / 2));
    kbCanvas.print(label);
}

void loadWifiCreds() {
    prefs.begin("wificfg", true);
    savedSsid = prefs.getString("ssid", DEFAULT_WIFI_SSID);
    savedPass = prefs.getString("pass", DEFAULT_WIFI_PASS);
    prefs.end();
}

void saveWifiCreds(const String& s, const String& p) {
    savedSsid = s;
    savedPass = p;
    prefs.begin("wificfg", false);
    prefs.putString("ssid", s);
    prefs.putString("pass", p);
    prefs.end();
}

void ensureSoftApRunning() {
    if ((WiFi.getMode() & WIFI_AP) == 0) {
        WiFi.mode(WIFI_AP_STA);
    }
    int ch = (WiFi.status() == WL_CONNECTED) ? WiFi.channel() : WIFI_CHANNEL;
    WiFi.softAP(AP_SSID, AP_PASS, ch);
}

// =========================================================================
// 6. HỆ THỐNG ÂM THANH 8-BIT ĐA GIAI ĐIỆU & ĐIỀU CHỈNH ÂM LƯỢNG 10%/MỨC
// =========================================================================
enum SfxType : uint8_t {
    SFX_NONE = 0, SFX_MOVE, SFX_EAT, SFX_CLEAR, SFX_BOUNCE, SFX_WIN, SFX_GAMEOVER
};
volatile uint8_t pendingSfx = SFX_NONE;
volatile bool bgmEnabled    = true;
volatile int  soundVolLevel = 6; // 0..10 tương ứng 0%..100% (bước 10%/mức)

// Bảng độ rộng xung PWM (8-bit, max 128 = 50% duty cycle) cho 11 mức: 0%, 10%, ..., 100%
constexpr uint8_t VOL_DUTY_TABLE[11] = {0, 3, 7, 13, 21, 32, 46, 62, 82, 104, 128};

struct NoteStep {
    uint16_t freq;
    uint16_t durMs;
};

// Nhạc 1: SNAKE - Giai điệu Korobeiniki (Russian Folk Arcade)
const NoteStep BGM_SNAKE[] = {
    {659, 240}, {494, 120}, {523, 120}, {587, 240}, {523, 120}, {494, 120},
    {440, 240}, {440, 120}, {523, 120}, {659, 240}, {587, 120}, {523, 120},
    {494, 360}, {523, 120}, {587, 240}, {659, 240},
    {523, 240}, {440, 240}, {440, 240}, {0,   120},
    {587, 240}, {698, 120}, {880, 240}, {784, 120}, {698, 120},
    {659, 360}, {523, 120}, {659, 240}, {587, 120}, {523, 120},
    {494, 240}, {494, 120}, {523, 120}, {587, 240}, {659, 240},
    {523, 240}, {440, 240}, {440, 240}, {0,   180}
};
constexpr int LEN_SNAKE = sizeof(BGM_SNAKE) / sizeof(BGM_SNAKE[0]);

// Nhạc 2: TETRIS - Giai điệu Minuet in G (Bach Chiptune)
const NoteStep BGM_TETRIS[] = {
    {587, 260}, {392, 130}, {440, 130}, {494, 130}, {523, 130},
    {587, 260}, {392, 260}, {392, 260},
    {659, 260}, {523, 130}, {587, 130}, {659, 130}, {740, 130},
    {784, 260}, {392, 260}, {392, 260},
    {523, 260}, {587, 130}, {523, 130}, {494, 130}, {440, 130},
    {494, 260}, {523, 130}, {494, 130}, {440, 130}, {392, 130},
    {370, 260}, {392, 130}, {440, 130}, {392, 130}, {330, 130},
    {440, 480}, {0,   140}
};
constexpr int LEN_TETRIS = sizeof(BGM_TETRIS) / sizeof(BGM_TETRIS[0]);

// Nhạc 3: PAC-MAN - Giai điệu Arcade Chase
const NoteStep BGM_PACMAN[] = {
    {494, 110}, {988, 110}, {740, 110}, {622, 110}, {988, 65}, {740, 155}, {622, 220},
    {523, 110}, {1047,110}, {784, 110}, {659, 110}, {1047,65}, {784, 155}, {659, 220},
    {494, 110}, {988, 110}, {740, 110}, {622, 110}, {988, 65}, {740, 155}, {622, 220},
    {622, 75},  {659, 75},  {698, 110}, {698, 75},  {740, 75}, {784, 110}, {784, 75}, {831, 75}, {880, 120}, {988, 220},
    {0,   140}
};
constexpr int LEN_PACMAN = sizeof(BGM_PACMAN) / sizeof(BGM_PACMAN[0]);

// Nhạc 4: BREAKOUT - Giai điệu Synthwave Arpeggio (C - Am - F - G)
const NoteStep BGM_BREAKOUT[] = {
    {523, 130}, {659, 130}, {784, 130}, {1047,130}, {784, 130}, {659, 130},
    {440, 130}, {523, 130}, {659, 130}, {880, 130}, {659, 130}, {523, 130},
    {349, 130}, {440, 130}, {523, 130}, {698, 130}, {523, 130}, {440, 130},
    {392, 130}, {494, 130}, {587, 130}, {784, 130}, {659, 130}, {587, 130},
    {523, 260}, {0,   120}
};
constexpr int LEN_BREAKOUT = sizeof(BGM_BREAKOUT) / sizeof(BGM_BREAKOUT[0]);

void loadSoundSettings() {
    prefs.begin("soundcfg", true);
    bgmEnabled    = prefs.getBool("bgm", true);
    soundVolLevel = constrain(prefs.getInt("vol10", 6), 0, 10);
    prefs.end();
}

void saveSoundSettings() {
    prefs.begin("soundcfg", false);
    prefs.putBool("bgm", bgmEnabled);
    prefs.putInt("vol10", soundVolLevel);
    prefs.end();
}

void triggerSfx(uint8_t sfx) {
    if (soundVolLevel > 0) pendingSfx = sfx;
}

void buzzerTone(int freq) {
    int vol = constrain(soundVolLevel, 0, 10);
    if (freq <= 0 || vol <= 0) {
        ledcWrite(BUZZER_PIN, 0);
        return;
    }
    ledcChangeFrequency(BUZZER_PIN, freq, 8);
    ledcWrite(BUZZER_PIN, VOL_DUTY_TABLE[vol]);
}

void playToneHz(int freq, int durMs) {
    buzzerTone(freq);
    vTaskDelay(pdMS_TO_TICKS(durMs));
    buzzerTone(0);
}

void processAudioCore0() {
    static int bgmIdx = 0;
    static State lastBgmGame = MENU;
    static uint32_t nextStepAt = 0;
    static bool isGapPhase = false;

    uint8_t sfx = pendingSfx;
    if (sfx != SFX_NONE) {
        pendingSfx = SFX_NONE;
        switch (sfx) {
            case SFX_MOVE:     playToneHz(980, 16); break;
            case SFX_EAT:      playToneHz(587, 38); playToneHz(880, 55); break;
            case SFX_CLEAR:    playToneHz(523, 45); playToneHz(659, 45); playToneHz(784, 45); playToneHz(1047, 85); break;
            case SFX_BOUNCE:   playToneHz(460, 22); break;
            case SFX_WIN:      playToneHz(523, 75); playToneHz(659, 75); playToneHz(784, 75); playToneHz(1047, 190); break;
            case SFX_GAMEOVER: playToneHz(392, 110); playToneHz(349, 110); playToneHz(330, 110); playToneHz(262, 260); break;
            default: break;
        }
        nextStepAt = millis() + 60;
        isGapPhase = false;
        return;
    }

    State cur = state;
    bool inGame = (cur == SNAKE || cur == TETRIS || cur == PACMAN || cur == BREAKOUT);

    if (!bgmEnabled || soundVolLevel == 0 || !inGame) {
        buzzerTone(0);
        lastBgmGame = MENU;
        return;
    }

    if (cur != lastBgmGame) {
        lastBgmGame = cur;
        bgmIdx = 0;
        isGapPhase = false;
        nextStepAt = millis();
    }

    uint32_t now = millis();
    if (now < nextStepAt) return;

    const NoteStep* song = BGM_SNAKE;
    int songLen = LEN_SNAKE;
    if (cur == TETRIS)        { song = BGM_TETRIS;   songLen = LEN_TETRIS; }
    else if (cur == PACMAN)   { song = BGM_PACMAN;   songLen = LEN_PACMAN; }
    else if (cur == BREAKOUT) { song = BGM_BREAKOUT; songLen = LEN_BREAKOUT; }

    if (bgmIdx >= songLen) bgmIdx = 0;
    uint16_t f = song[bgmIdx].freq;
    uint16_t d = song[bgmIdx].durMs;

    if (!isGapPhase) {
        uint16_t playMs = max((uint16_t)20, (uint16_t)((d * 85) / 100));
        buzzerTone(f);
        nextStepAt = now + playMs;
        isGapPhase = true;
    } else {
        uint16_t gapMs = max((uint16_t)10, (uint16_t)(d - (d * 85) / 100));
        buzzerTone(0);
        nextStepAt = now + gapMs;
        isGapPhase = false;
        bgmIdx = (bgmIdx + 1) % songLen;
    }
}

// =========================================================================
// 7. BẢNG XẾP HẠNG MẠNG NỘI BỘ (LAN UDP BROADCAST + LƯU NVS TỐI ĐA 10 NGƯỜI)
// =========================================================================
struct LeaderEntry {
    String player;
    String game;
    int score;
    int level;
};
std::vector<LeaderEntry> leaderboard;
volatile bool leaderboardDirtySave = false;
volatile bool lanBroadcastPending = false;
int lbScrollOffset = 0;

void loadLeaderboardNvs() {
    prefs.begin("lan_top10", true);
    String raw = prefs.getString("data", "");
    prefs.end();

    if (raw.length() > 0) {
        JsonDocument doc;
        if (!deserializeJson(doc, raw)) {
            JsonArray arr = doc.as<JsonArray>();
            leaderboard.clear();
            for (JsonObject obj : arr) {
                leaderboard.push_back({
                    obj["p"] | "Player",
                    obj["g"] | "GAME",
                    obj["s"] | 0,
                    obj["l"] | 1
                });
            }
        }
    }
}

void saveLeaderboardNvs() {
    JsonDocument doc;
    JsonArray arr = doc.to<JsonArray>();
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
        for (size_t i = 0; i < leaderboard.size() && i < MAX_TOP_PLAYERS; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["p"] = leaderboard[i].player;
            obj["g"] = leaderboard[i].game;
            obj["s"] = leaderboard[i].score;
            obj["l"] = leaderboard[i].level;
        }
        xSemaphoreGive(dataMutex);
    }
    String out;
    serializeJson(doc, out);
    prefs.begin("lan_top10", false);
    prefs.putString("data", out);
    prefs.end();
}

bool insertLeaderEntry(const String& p, const String& g, int s, int lv) {
    if (s <= 0) return false;
    bool changed = false;
    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
        bool found = false;
        for (auto& item : leaderboard) {
            if (item.player == p && item.game == g) {
                found = true;
                if (s > item.score) {
                    item.score = s;
                    item.level = lv;
                    changed = true;
                }
                break;
            }
        }
        if (!found) {
            leaderboard.push_back({p, g, s, lv});
            changed = true;
        }
        if (changed) {
            std::sort(leaderboard.begin(), leaderboard.end(), [](const LeaderEntry& a, const LeaderEntry& b) {
                return a.score > b.score;
            });
            if (leaderboard.size() > MAX_TOP_PLAYERS) {
                leaderboard.resize(MAX_TOP_PLAYERS);
            }
            leaderboardDirtySave = true;
        }
        xSemaphoreGive(dataMutex);
    }
    return changed;
}

void broadcastLanLeaderboard() {
    JsonDocument doc;
    doc["magic"] = "ESP32_LAN_TOP10";
    JsonArray arr = doc["list"].to<JsonArray>();

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20))) {
        for (size_t i = 0; i < leaderboard.size() && i < MAX_TOP_PLAYERS; i++) {
            JsonObject obj = arr.add<JsonObject>();
            obj["p"] = leaderboard[i].player;
            obj["g"] = leaderboard[i].game;
            obj["s"] = leaderboard[i].score;
            obj["l"] = leaderboard[i].level;
        }
        xSemaphoreGive(dataMutex);
    }

    char packet[768];
    size_t len = serializeJson(doc, packet, sizeof(packet));
    if (len == 0) return;

    if (WiFi.status() == WL_CONNECTED) {
        IPAddress bcastSta = WiFi.localIP();
        bcastSta[3] = 255;
        lanUdp.beginPacket(bcastSta, LAN_UDP_PORT);
        lanUdp.write((const uint8_t*)packet, len);
        lanUdp.endPacket();
    }
    if ((WiFi.getMode() & WIFI_AP) != 0) {
        IPAddress bcastAp = WiFi.softAPIP();
        bcastAp[3] = 255;
        lanUdp.beginPacket(bcastAp, LAN_UDP_PORT);
        lanUdp.write((const uint8_t*)packet, len);
        lanUdp.endPacket();
    }
}

void receiveLanLeaderboard() {
    int packetSize = lanUdp.parsePacket();
    if (packetSize <= 0) return;

    char incoming[800];
    int len = lanUdp.read(incoming, sizeof(incoming) - 1);
    if (len <= 0) return;
    incoming[len] = '\0';

    JsonDocument doc;
    if (!deserializeJson(doc, incoming)) {
        if (String(doc["magic"] | "") == "ESP32_LAN_TOP10") {
            JsonArray arr = doc["list"].as<JsonArray>();
            for (JsonObject obj : arr) {
                insertLeaderEntry(
                    obj["p"] | "Player",
                    obj["g"] | "GAME",
                    obj["s"] | 0,
                    obj["l"] | 1
                );
            }
        }
    }
}

void recordHighScoreLocalAndLan(const char* gameName, const String& playerName, int sc, int lv) {
    if (insertLeaderEntry(playerName, gameName, sc, lv)) {
        lanBroadcastPending = true;
    }
}

// =========================================================================
// 8. WEB CONTROLLER + HIỂN THỊ TRỰC TIẾP MÀN HÌNH TFT 128x160
// =========================================================================
volatile int p2JoyX = 0, p2JoyY = 0;
volatile bool p2Action = false;
volatile uint32_t p2LastPing = 0;
String p2PlayerName = "Player_2";

const uint8_t BMP_HEADER_RGB565[66] = {
    0x42, 0x4D,
    0x42, 0xA0, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x42, 0x00, 0x00, 0x00,
    0x28, 0x00, 0x00, 0x00,
    0x80, 0x00, 0x00, 0x00,
    0x60, 0xFF, 0xFF, 0xFF,
    0x01, 0x00,
    0x10, 0x00,
    0x03, 0x00, 0x00, 0x00,
    0x00, 0xA0, 0x00, 0x00,
    0x13, 0x0B, 0x00, 0x00,
    0x13, 0x0B, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00,
    0x00, 0xF8, 0x00, 0x00,
    0xE0, 0x07, 0x00, 0x00,
    0x1F, 0x00, 0x00, 0x00
};

const char WEB_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no">
<title>ESP32 P2 Screen & Gamepad</title>
<style>
  body{background:#0d0d12;color:#fff;font-family:sans-serif;text-align:center;margin:0;padding:8px;user-select:none}
  h3{color:#0ff;margin:2px 0 6px;font-size:16px}
  .screen-wrap{display:inline-block;padding:4px;background:#1a1a24;border:2px solid #0ff;border-radius:10px;box-shadow:0 0 12px rgba(0,255,255,0.3)}
  #tft{width:224px;height:280px;image-rendering:pixelated;display:block;background:#000;border-radius:6px}
  .namebox{margin:8px 0}
  input{padding:6px 8px;border-radius:6px;border:1px solid #0ff;background:#1b1b26;color:#fff;width:130px;font-size:13px}
  .savebtn{padding:6px 10px;border-radius:6px;border:none;background:#0f0;color:#000;font-weight:bold;font-size:13px}
  .controls{display:flex;justify-content:center;align-items:center;gap:18px;margin-top:8px}
  .grid{display:grid;grid-template-columns:repeat(3,56px);grid-gap:6px}
  .pad{height:56px;font-size:20px;font-weight:bold;border:2px solid #0ff;border-radius:12px;background:#1e1e2a;color:#fff;touch-action:manipulation}
  .pad:active{background:#0ff;color:#000}
  .act{width:86px;height:86px;border:2px solid #f0f;border-radius:50%;background:#1e1e2a;color:#fff;font-size:13px;font-weight:bold;touch-action:manipulation}
  .act:active{background:#f0f;color:#000}
</style>
</head>
<body>
  <h3>ESP32-S3 LIVE SCREEN &amp; P2 PAD</h3>
  <div class="screen-wrap">
    <img id="tft" src="/screen.bmp" alt="TFT Screen">
  </div>
  <div class="namebox">
    <input id="pname" placeholder="Ten nguoi choi 2" maxlength="14">
    <button class="savebtn" onclick="setName()">LUU TEN</button>
  </div>
  <div class="controls">
    <div class="grid">
      <div></div><button class="pad" ontouchstart="s(0,-1)" ontouchend="s(0,0)" onmousedown="s(0,-1)" onmouseup="s(0,0)">&#9650;</button><div></div>
      <button class="pad" ontouchstart="s(-1,0)" ontouchend="s(0,0)" onmousedown="s(-1,0)" onmouseup="s(0,0)">&#9664;</button>
      <div></div>
      <button class="pad" ontouchstart="s(1,0)" ontouchend="s(0,0)" onmousedown="s(1,0)" onmouseup="s(0,0)">&#9654;</button>
      <div></div><button class="pad" ontouchstart="s(0,1)" ontouchend="s(0,0)" onmousedown="s(0,1)" onmouseup="s(0,0)">&#9660;</button><div></div>
    </div>
    <button class="act" ontouchstart="a(1)" ontouchend="a(0)" onmousedown="a(1)" onmouseup="a(0)">ACTION<br>BOOST</button>
  </div>
<script>
  const img = document.getElementById('tft');
  function refreshScreen(){
    img.src = '/screen.bmp?t=' + Date.now();
  }
  img.onload = () => setTimeout(refreshScreen, 50);
  img.onerror = () => setTimeout(refreshScreen, 300);

  function s(x,y){fetch(`/ctrl?x=${x}&y=${y}`).catch(()=>{});}
  function a(v){fetch(`/ctrl?a=${v}`).catch(()=>{});}
  function setName(){
    let n=document.getElementById('pname').value.trim();
    if(n) fetch(`/name?n=${encodeURIComponent(n)}`).then(()=>alert('Da luu ten: '+n));
  }
  document.addEventListener('keydown', e => {
    if(e.repeat) return;
    if(e.key==='ArrowUp'||e.key==='w') s(0,-1);
    else if(e.key==='ArrowDown'||e.key==='s') s(0,1);
    else if(e.key==='ArrowLeft'||e.key==='a') s(-1,0);
    else if(e.key==='ArrowRight'||e.key==='d') s(1,0);
    else if(e.key===' ') a(1);
  });
  document.addEventListener('keyup', e => {
    if(['ArrowUp','ArrowDown','ArrowLeft','ArrowRight','w','s','a','d'].includes(e.key)) s(0,0);
    else if(e.key===' ') a(0);
  });
</script>
</body>
</html>
)rawliteral";

void handleScreenBmp() {
    if (pauseNetworkTask || state == WIFI_SCAN || state == WIFI_PASS_INPUT) {
        server.send(204, "text/plain", "");
        return;
    }

    static uint8_t bmpPacket[66 + 128 * 160 * 2];
    memcpy(bmpPacket, BMP_HEADER_RGB565, 66);

    if (frameMutex && xSemaphoreTake(frameMutex, pdMS_TO_TICKS(15)) == pdTRUE) {
        memcpy(bmpPacket + 66, webMirrorBuf, 128 * 160 * 2);
        xSemaphoreGive(frameMutex);
    }

    server.sendHeader("Cache-Control", "no-store, no-cache, must-revalidate, max-age=0");
    server.send_P(200, "image/bmp", (const char*)bmpPacket, sizeof(bmpPacket));
}

void setupWebServer() {
    server.on("/", []() { server.send_P(200, "text/html", WEB_HTML); });
    server.on("/screen.bmp", handleScreenBmp);
    server.on("/ctrl", []() {
        p2LastPing = millis();
        if (server.hasArg("x")) p2JoyX = constrain(server.arg("x").toInt(), -1, 1);
        if (server.hasArg("y")) p2JoyY = constrain(server.arg("y").toInt(), -1, 1);
        if (server.hasArg("a")) p2Action = (server.arg("a").toInt() != 0);
        server.send(200, "text/plain", "OK");
    });
    server.on("/name", []() {
        if (server.hasArg("n")) {
            String n = server.arg("n");
            n.trim();
            if (n.length() > 0) {
                if (n.length() > 14) n = n.substring(0, 14);
                p2PlayerName = n;
            }
        }
        server.send(200, "text/plain", "OK");
    });
    server.begin();
}

// =========================================================================
// 9. PPT REMOTE (ESP-NOW)
// =========================================================================
#define IMG_W 158
#define IMG_H 126

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  type;
    uint16_t param1;
    uint16_t param2;
    uint8_t  len;
    uint8_t  data[MAX_PAYLOAD];
} DataPacket;

typedef struct __attribute__((packed)) {
    uint16_t magic;
    uint8_t  cmd;
    uint8_t  checksum;
} CmdPacket;

uint16_t currImgBuffer[IMG_W * IMG_H];
uint16_t nextImgBuffer[IMG_W * IMG_H];
uint16_t portScaledBuf[126 * 78];
char slideTextBuf[2048];
volatile bool textUpdated = false, currImgDirty = false, nextImgDirty = false;
unsigned long lastPacketMs = 0;

enum OrientationMode { ORIENT_LANDSCAPE, ORIENT_PORTRAIT };
enum ContentMode     { CONTENT_CURR_SLIDE, CONTENT_TEXT, CONTENT_NEXT_SLIDE };
OrientationMode orientMode  = ORIENT_LANDSCAPE;
ContentMode     contentMode = CONTENT_CURR_SLIDE;
int textScrollLine = 0, totalWrappedLines = 0;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void onDataRecv(const esp_now_recv_info_t *info, const uint8_t *incomingData, int len) {
    if (len != sizeof(DataPacket)) return;
    DataPacket pkt;
    memcpy(&pkt, incomingData, sizeof(DataPacket));
    if (pkt.magic != 0x5A77) return;
    lastPacketMs = millis();

    if (pkt.type == 1 || pkt.type == 4) {
        uint16_t row = pkt.param1, byteOffset = pkt.param2;
        if (row < IMG_H && (byteOffset + pkt.len) <= (IMG_W * 2)) {
            portENTER_CRITICAL_ISR(&mux);
            uint16_t* target = (pkt.type == 1) ? currImgBuffer : nextImgBuffer;
            uint8_t* dst = (uint8_t*)&target[row * IMG_W] + byteOffset;
            memcpy(dst, pkt.data, pkt.len);
            if (row == IMG_H - 1 && byteOffset + pkt.len == IMG_W * 2) {
                if (pkt.type == 1) currImgDirty = true;
                else nextImgDirty = true;
            }
            portEXIT_CRITICAL_ISR(&mux);
        }
    } else if (pkt.type == 2) {
        uint16_t offset = pkt.param1, totalLen = pkt.param2;
        if (totalLen < sizeof(slideTextBuf) - 1 && (offset + pkt.len) <= totalLen) {
            memcpy(slideTextBuf + offset, pkt.data, pkt.len);
            if (offset + pkt.len == totalLen) {
                slideTextBuf[totalLen] = '\0';
                textUpdated = true;
            }
        }
    }
}

void sendPptCommand(uint8_t cmdCode) {
    CmdPacket pkt = {0x5A78, cmdCode, (uint8_t)(cmdCode ^ 0xAA)};
    esp_now_send(c3PeerMac, (uint8_t*)&pkt, sizeof(CmdPacket));
}

// =========================================================================
// 10. TASK CHẠY TRÊN CORE 0 (WIFI NỘI BỘ, LAN UDP TOP 10, WEB SERVER, AUDIO)
// =========================================================================
void networkAudioTask(void* pvParameters) {
    WiFi.mode(WIFI_AP_STA);
    WiFi.setSleep(false);
    WiFi.softAP(AP_SSID, AP_PASS, WIFI_CHANNEL);
    if (savedSsid.length() > 0 && savedSsid != "YOUR_WIFI_SSID") {
        WiFi.begin(savedSsid.c_str(), savedPass.c_str());
    }

    if (esp_now_init() == ESP_OK) {
        esp_now_register_recv_cb(onDataRecv);
        esp_now_peer_info_t peerInfo = {};
        memcpy(peerInfo.peer_addr, c3PeerMac, 6);
        peerInfo.channel = WIFI_CHANNEL;
        peerInfo.encrypt = false;
        esp_now_add_peer(&peerInfo);
    }

    setupWebServer();
    lanUdp.begin(LAN_UDP_PORT);

    uint32_t lastPeriodicSync = 0;

    for (;;) {
        processAudioCore0();

        if (pauseNetworkTask) {
            vTaskDelay(pdMS_TO_TICKS(10));
            continue;
        }

        server.handleClient();
        receiveLanLeaderboard();

        if (leaderboardDirtySave) {
            leaderboardDirtySave = false;
            saveLeaderboardNvs();
        }

        if (lanBroadcastPending || (millis() - lastPeriodicSync > 15000)) {
            lanBroadcastPending = false;
            lastPeriodicSync = millis();
            broadcastLanLeaderboard();
        }

        vTaskDelay(pdMS_TO_TICKS(4));
    }
}

// =========================================================================
// 11. BIẾN & HÀM TIỆN ÍCH GAME (CORE 1)
// =========================================================================
const char* gameNames[]     = {"SNAKE", "TETRIS", "PAC-MAN", "BREAKOUT"};
const uint16_t gameColors[] = {GREEN, CYAN, YELLOW, ORANGE};

int menuIndex = 0, settingIndex = 0, soundMenuIndex = 0, resultChoice = 0;
int resetPressCount = 0;
uint32_t resetPressAt = 0;
bool gameWon = false;
int score = 0, scoreP2 = 0;
int best[4]  = {0, 0, 0, 0};
constexpr int MAX_LEVEL = 10;
int level[4] = {1, 1, 1, 1};
bool newRecord = false;
uint32_t lastInput = 0;

int centerX = 2048, centerY = 2048;
int joyX = 0, joyY = 0;
bool rawSW = false, stableSW = false, shortPress = false, longPress = false;
uint32_t rawChanged = 0, pressAt = 0;
bool longFired = false;
constexpr int DEADZONE = 650;
constexpr uint32_t DEBOUNCE = 28, LONG_MS = 1000;

int axis(int pin, int center) {
    int d = analogRead(pin) - center;
    return abs(d) < DEADZONE ? 0 : (d < 0 ? -1 : 1);
}

void updateInput() {
    uint32_t now = millis();
    int prevX = joyX, prevY = joyY;
    joyX = axis(JOY_X_PIN, centerX);
    joyY = axis(JOY_Y_PIN, centerY);

    if ((joyX != 0 && prevX == 0) || (joyY != 0 && prevY == 0)) {
        if (state == MENU || state == SETTING || state == SOUND_MENU || state == LEADERBOARD ||
            state == RESULT || state == WIFI_SCAN || state == WIFI_PASS_INPUT) {
            triggerSfx(SFX_MOVE);
        }
    }

    shortPress = longPress = false;
    bool r = (digitalRead(JOY_SW_PIN) == LOW);
    if (r != rawSW) { rawSW = r; rawChanged = now; }
    if (rawSW != stableSW && now - rawChanged >= DEBOUNCE) {
        stableSW = rawSW;
        if (stableSW) { pressAt = now; longFired = false; }
        else if (!longFired && now - pressAt < LONG_MS) shortPress = true;
    }
    if (stableSW && !longFired && now - pressAt >= LONG_MS) {
        longFired = true;
        longPress = true;
    }
}

void calibrate() {
    long sx = 0, sy = 0;
    for (int i = 0; i < 35; i++) {
        sx += analogRead(JOY_X_PIN);
        sy += analogRead(JOY_Y_PIN);
        delay(3);
    }
    centerX = sx / 35;
    centerY = sy / 35;
}

void canvasCentered(const char* s, int y, uint16_t c, uint8_t size = 1) {
    canvas.setTextSize(size);
    canvas.setTextColor(c);
    int16_t x1, y1;
    uint16_t tw, th;
    canvas.getTextBounds(s, 0, y, &x1, &y1, &tw, &th);
    canvas.setCursor((W - tw) / 2, y);
    canvas.print(s);
}

float tocDoGame(int lv) {
    return 0.5f + (constrain(lv, 1, MAX_LEVEL) - 1) * 0.25f;
}

uint16_t levelColor(int lv) {
    const uint16_t palette[10] = {GREEN, CYAN, YELLOW, ORANGE, MAGENTA, BLUE, RED, PINK, WHITE, 0x07E0};
    return palette[(constrain(lv, 1, MAX_LEVEL) - 1) % 10];
}

int gameSlot(State s) {
    switch (s) {
        case SNAKE:  return 0;
        case TETRIS: return 1;
        case PACMAN: return 2;
        default:     return 3;
    }
}

const char* keyFor(State s) {
    switch (s) {
        case SNAKE:  return "snake";
        case TETRIS: return "tetris";
        case PACMAN: return "pacman";
        default:     return "breakout";
    }
}

void loadBest() {
    prefs.begin("minigame", true);
    best[0]  = prefs.getInt("snake", 0);
    best[1]  = prefs.getInt("tetris", 0);
    best[2]  = prefs.getInt("pacman", 0);
    best[3]  = prefs.getInt("breakout", 0);
    level[0] = constrain(prefs.getInt("snakeLv", 1), 1, MAX_LEVEL);
    level[1] = constrain(prefs.getInt("tetrisLv", 1), 1, MAX_LEVEL);
    level[2] = constrain(prefs.getInt("pacmanLv", 1), 1, MAX_LEVEL);
    level[3] = constrain(prefs.getInt("breakoutLv", 1), 1, MAX_LEVEL);
    prefs.end();
}

void saveLevels() {
    prefs.begin("minigame", false);
    prefs.putInt("snakeLv", level[0]);
    prefs.putInt("tetrisLv", level[1]);
    prefs.putInt("pacmanLv", level[2]);
    prefs.putInt("breakoutLv", level[3]);
    prefs.end();
}

void drawCanvasHeader(const char* title, uint16_t col) {
    int slot = gameSlot(state);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, levelColor(level[slot]));
    canvas.drawFastHLine(SAFE_X, 20, SAFE_W, levelColor(level[slot]));
    canvas.setTextSize(1);
    canvas.setTextColor(col);
    canvas.setCursor(6, 8);
    canvas.print(title);

    canvas.setTextColor(YELLOW);
    if (twoPlayerMode && (state == SNAKE || state == BREAKOUT)) {
        canvas.setCursor(62, 8);
        canvas.printf("P1:%d P2:%d", score, scoreP2);
    } else {
        canvas.setCursor(82, 8);
        canvas.printf("S:%d", score);
    }
}

// =========================================================================
// 12. MENU CHÍNH (4 GAME + PPT REMOTE + SETTING + VẠCH SÓNG WIFI)
// =========================================================================
constexpr int MENU_COUNT = 6;

void enterMenu() {
    state = MENU;
    tft.setRotation(0);
}

void renderMenu() {
    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);

    canvas.setTextSize(1);
    canvas.setTextColor(CYAN);
    canvas.setCursor(8, 7);
    canvas.print("MINI GAME");
    drawWifiSignalIcon(110, 5);
    canvas.drawFastHLine(SAFE_X, 18, SAFE_W, GRAY);

    const char* names[MENU_COUNT] = {
        "SNAKE",
        "TETRIS",
        "PAC-MAN",
        "BREAKOUT",
        "PPT REMOTE",
        "SETTING"
    };

    for (int i = 0; i < MENU_COUNT; i++) {
        int y = 24 + i * 21;
        uint16_t c = (i < 4) ? gameColors[i] :
                     (i == 4) ? MAGENTA : WHITE;

        if (i == menuIndex) {
            canvas.fillRoundRect(6, y, 116, 18, 4, c);
            canvas.setTextColor(BLACK);
        } else {
            canvas.drawRoundRect(6, y, 116, 18, 4, c);
            canvas.setTextColor(c);
        }
        canvas.setCursor(12, y + 5);
        canvas.print(names[i]);

        if (i < 4) {
            canvas.setCursor(98, y + 5);
            canvas.printf("L%d", level[i]);
        } else if (i == 5 && twoPlayerMode) {
            canvas.setCursor(98, y + 5);
            canvas.print("2P");
        }
    }
    flushCanvas();
}

// =========================================================================
// 13. MÀN HÌNH SETTING & SOUND MENU
// =========================================================================
constexpr int SETTING_COUNT = 5;

void enterSetting() {
    ensureSoftApRunning();
    pauseNetworkTask = false;
    state = SETTING;
    settingIndex = 0;
    resetPressCount = 0;
    tft.setRotation(0);
}

void renderSetting() {
    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, WHITE);

    canvas.setTextSize(1);
    canvas.setTextColor(YELLOW);
    canvas.setCursor(8, 7);
    canvas.print("SETTING");
    drawWifiSignalIcon(110, 5);
    canvas.drawFastHLine(SAFE_X, 18, SAFE_W, WHITE);

    const char* items[SETTING_COUNT] = {
        "WIFI",
        "SOUND",
        twoPlayerMode ? "MODE: 2 PLAYERS" : "MODE: 1 PLAYER",
        "LAN TOP 10",
        "RESET DATA x5"
    };
    const uint16_t cols[SETTING_COUNT] = {GREEN, ORANGE, PINK, CYAN, RED};

    for (int i = 0; i < SETTING_COUNT; i++) {
        int y = 21 + i * 16;
        if (i == settingIndex) {
            canvas.fillRoundRect(6, y, 116, 14, 3, cols[i]);
            canvas.setTextColor(BLACK);
        } else {
            canvas.drawRoundRect(6, y, 116, 14, 3, cols[i]);
            canvas.setTextColor(cols[i]);
        }
        canvas.setCursor(11, y + 3);
        canvas.print(items[i]);

        if (i == 0) {
            canvas.setCursor(84, y + 3);
            canvas.print(WiFi.status() == WL_CONNECTED ? "ON" : "OFF");
        } else if (i == 1) {
            canvas.setCursor(84, y + 3);
            if (!bgmEnabled || soundVolLevel == 0) canvas.print("OFF");
            else canvas.printf("%d%%", soundVolLevel * 10);
        } else if (i == 4) {
            canvas.setCursor(98, y + 3);
            canvas.printf("%d/5", resetPressCount);
        }
    }

    canvas.drawFastHLine(SAFE_X, 103, SAFE_W, GRAY);
    canvas.setTextColor(CYAN);
    canvas.setCursor(6, 106);
    canvas.print("AP IP: ");
    canvas.setTextColor(YELLOW);
    canvas.print(WiFi.softAPIP().toString());

    canvas.setTextColor(GREEN);
    canvas.setCursor(6, 117);
    if (WiFi.status() == WL_CONNECTED) {
        canvas.print("LAN: ");
        canvas.setTextColor(WHITE);
        canvas.print(WiFi.localIP().toString());
    } else {
        canvas.setTextColor(ORANGE);
        canvas.print("LAN: Chua ket noi");
    }

    canvasCentered("HOLD NUT: QUAY LAI", 147, GRAY, 1);
    flushCanvas();
}

void enterSoundMenu() {
    state = SOUND_MENU;
    soundMenuIndex = 0;
    tft.setRotation(0);
}

void renderSoundMenu() {
    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, ORANGE);
    canvasCentered("SOUND SETTINGS", 6, YELLOW, 1);
    canvas.drawFastHLine(SAFE_X, 18, SAFE_W, ORANGE);

    // Mục 0: Bật/Tắt nhạc nền BGM
    int y0 = 28;
    uint16_t c0 = bgmEnabled ? GREEN : RED;
    if (soundMenuIndex == 0) {
        canvas.fillRoundRect(6, y0, 116, 20, 4, c0);
        canvas.setTextColor(BLACK);
    } else {
        canvas.drawRoundRect(6, y0, 116, 20, 4, c0);
        canvas.setTextColor(c0);
    }
    canvas.setCursor(12, y0 + 6);
    canvas.printf("NHAC NEN: %s", bgmEnabled ? "ON" : "OFF");

    // Mục 1: Điều chỉnh âm lượng (0% - 100%, bước 10% mỗi mức)
    int y1 = 56;
    if (soundMenuIndex == 1) {
        canvas.fillRoundRect(6, y1, 116, 38, 4, CYAN);
        canvas.setTextColor(BLACK);
    } else {
        canvas.drawRoundRect(6, y1, 116, 38, 4, CYAN);
        canvas.setTextColor(CYAN);
    }
    canvas.setCursor(12, y1 + 5);
    canvas.printf("AM LUONG: %d%%", soundVolLevel * 10);

    // Thanh vạch âm lượng 10 nấc (10%/vạch)
    for (int b = 0; b < 10; b++) {
        int bx = 12 + b * 10;
        int by = y1 + 19;
        uint16_t barCol = (b < soundVolLevel)
            ? ((soundMenuIndex == 1) ? NAVY : YELLOW)
            : ((soundMenuIndex == 1) ? 0x4208 : 0x2104);
        canvas.fillRect(bx, by, 8, 12, barCol);
    }

    // Mục 2: Quay lại Setting
    int y2 = 104;
    if (soundMenuIndex == 2) {
        canvas.fillRoundRect(6, y2, 116, 20, 4, WHITE);
        canvas.setTextColor(BLACK);
    } else {
        canvas.drawRoundRect(6, y2, 116, 20, 4, WHITE);
        canvas.setTextColor(WHITE);
    }
    canvasCentered("BACK TO SETTING", y2 + 6, (soundMenuIndex == 2) ? BLACK : WHITE, 1);

    flushCanvas();
}

void stepSoundMenu() {
    if (longPress) {
        saveSoundSettings();
        enterSetting();
        return;
    }

    if (joyY && millis() - lastInput > 160) {
        lastInput = millis();
        soundMenuIndex = (soundMenuIndex + (joyY > 0 ? 1 : 2)) % 3;
    }

    if (joyX && millis() - lastInput > 130) {
        lastInput = millis();
        if (soundMenuIndex == 0) {
            bgmEnabled = !bgmEnabled;
            saveSoundSettings();
            triggerSfx(SFX_EAT);
        } else if (soundMenuIndex == 1) {
            soundVolLevel = constrain(soundVolLevel + joyX, 0, 10);
            saveSoundSettings();
            if (soundVolLevel > 0) triggerSfx(SFX_EAT);
        }
    }

    if (shortPress) {
        if (soundMenuIndex == 0) {
            bgmEnabled = !bgmEnabled;
            saveSoundSettings();
            triggerSfx(SFX_EAT);
        } else if (soundMenuIndex == 1) {
            soundVolLevel = (soundVolLevel + 1) % 11;
            saveSoundSettings();
            if (soundVolLevel > 0) triggerSfx(SFX_EAT);
        } else {
            saveSoundSettings();
            enterSetting();
            return;
        }
    }

    renderSoundMenu();
}

// =========================================================================
// 14. MỤC WIFI: QUÉT MẠNG + KẾT NỐI + NÚT "BACK TO SETTING"
// =========================================================================
void startWifiScan() {
    pauseNetworkTask = true;
    state = WIFI_SCAN;
    tft.setRotation(0);
    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GREEN);
    canvasCentered("SCANNING WIFI...", 70, YELLOW, 1);
    flushCanvas();

    // Tạm tắt SoftAP và chuyển về STA thuần để giải phóng bộ thu phát RF khỏi xung đột kênh
    WiFi.softAPdisconnect(true);
    WiFi.disconnect(true, true);
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);
    delay(150);

    scannedSsids.clear();
    scannedRssi.clear();
    WiFi.scanDelete();

    int n = WiFi.scanNetworks(false, false);
    for (int i = 0; i < n && i < 12; i++) {
        String s = WiFi.SSID(i);
        if (s.length() > 0) {
            scannedSsids.push_back(s);
            scannedRssi.push_back(WiFi.RSSI(i));
        }
    }
    WiFi.scanDelete();
    wifiListIdx = 0;
}

void stepWifiScan() {
    if (longPress) {
        enterSetting();
        return;
    }

    // Tổng số dòng = số mạng WiFi quét được + 1 dòng cuối "BACK TO SETTING"
    int totalItems = (int)scannedSsids.size() + 1;

    if (joyY && millis() - lastInput > 170) {
        lastInput = millis();
        wifiListIdx = (wifiListIdx + (joyY > 0 ? 1 : totalItems - 1)) % totalItems;
    }

    if (shortPress) {
        if (wifiListIdx == totalItems - 1) {
            // Chọn BACK TO SETTING
            enterSetting();
            return;
        } else if (!scannedSsids.empty()) {
            targetSsid = scannedSsids[wifiListIdx];
            inputPassword = "";
            state = WIFI_PASS_INPUT;
            tft.setRotation(1);
            vkb.begin(160, 128, 34, drawVirtualKeyCallback);
            vkb.resetCursor();
            vkb.setMode(MODE_LOWERCASE);
            triggerSfx(SFX_EAT);
            return;
        }
    }

    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GREEN);
    canvasCentered("WIFI", 5, YELLOW, 1);
    canvas.drawFastHLine(SAFE_X, 17, SAFE_W, GREEN);

    if (scannedSsids.empty()) {
        canvasCentered("Khong tim thay WiFi", 48, RED, 1);
        canvasCentered("2.4GHz xung quanh", 60, GRAY, 1);

        int yBack = 104;
        canvas.fillRoundRect(6, yBack, 116, 20, 4, WHITE);
        canvas.drawRoundRect(6, yBack, 116, 20, 4, WHITE);
        canvasCentered("BACK TO SETTING", yBack + 6, BLACK, 1);
    } else {
        int startIdx = max(0, wifiListIdx - 6);
        int endIdx   = min(totalItems, startIdx + 7);
        for (int i = startIdx; i < endIdx; i++) {
            int row = i - startIdx;
            int y = 21 + row * 18;
            bool isBackBtn = (i == totalItems - 1);

            if (isBackBtn) {
                if (i == wifiListIdx) {
                    canvas.fillRoundRect(5, y, 118, 16, 3, WHITE);
                    canvasCentered("BACK TO SETTING", y + 4, BLACK, 1);
                } else {
                    canvas.drawRoundRect(5, y, 118, 16, 3, WHITE);
                    canvasCentered("BACK TO SETTING", y + 4, WHITE, 1);
                }
            } else {
                if (i == wifiListIdx) {
                    canvas.fillRoundRect(5, y, 118, 16, 3, GREEN);
                    canvas.setTextColor(BLACK);
                } else {
                    canvas.drawRoundRect(5, y, 118, 16, 3, 0x2104);
                    canvas.setTextColor(WHITE);
                }
                String s = scannedSsids[i];
                if (s.length() > 14) s = s.substring(0, 14);
                canvas.setCursor(9, y + 4);
                canvas.print(s);

                canvas.setCursor(96, y + 4);
                canvas.printf("%ddB", scannedRssi[i]);
            }
        }
    }
    flushCanvas();
}

void connectToSelectedWifi(const String& ssid, const String& pass) {
    saveWifiCreds(ssid, pass);

    kbCanvas.fillScreen(BLACK);
    kbCanvas.drawRect(2, 2, 156, 124, GREEN);
    kbCanvas.setTextColor(YELLOW);
    kbCanvas.setCursor(16, 42);
    kbCanvas.print("Dang ket noi WiFi:");
    kbCanvas.setTextColor(CYAN);
    kbCanvas.setCursor(16, 58);
    kbCanvas.print(ssid);
    tft.drawRGBBitmap(0, 0, kbCanvas.getBuffer(), 160, 128);

    WiFi.disconnect(true, true);
    delay(150);
    WiFi.mode(WIFI_STA);
    WiFi.setSleep(false);

    if (pass.length() == 0) WiFi.begin(ssid.c_str());
    else WiFi.begin(ssid.c_str(), pass.c_str());

    uint32_t t0 = millis();
    int dots = 0;
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 10000) {
        delay(250);
        dots = (dots + 1) % 4;
        kbCanvas.fillRect(16, 76, 130, 14, BLACK);
        kbCanvas.setTextColor(WHITE);
        kbCanvas.setCursor(16, 76);
        kbCanvas.print("Vui long cho");
        for (int d = 0; d < dots; d++) kbCanvas.print(".");
        tft.drawRGBBitmap(0, 0, kbCanvas.getBuffer(), 160, 128);
    }

    kbCanvas.fillRect(10, 74, 140, 24, BLACK);
    if (WiFi.status() == WL_CONNECTED) {
        triggerSfx(SFX_WIN);
        kbCanvas.setTextColor(GREEN);
        kbCanvas.setCursor(16, 76);
        kbCanvas.print("KET NOI THANH CONG!");
        kbCanvas.setTextColor(YELLOW);
        kbCanvas.setCursor(16, 90);
        kbCanvas.print(WiFi.localIP().toString());
    } else {
        triggerSfx(SFX_GAMEOVER);
        kbCanvas.setTextColor(RED);
        kbCanvas.setCursor(16, 76);
        kbCanvas.print("KET NOI THAT BAI!");
        kbCanvas.setTextColor(GRAY);
        kbCanvas.setCursor(16, 90);
        kbCanvas.print("Kiem tra lai mat khau");
    }
    tft.drawRGBBitmap(0, 0, kbCanvas.getBuffer(), 160, 128);
    delay(1100);

    // Khởi động lại SoftAP đồng bộ kênh với Router vừa kết nối
    ensureSoftApRunning();
    pauseNetworkTask = false;
}

void stepWifiPassInput() {
    if (longPress) {
        enterSetting();
        return;
    }

    int moveX = joyY;
    int moveY = -joyX;

    if ((moveX != 0 || moveY != 0) && millis() - lastInput > 165) {
        lastInput = millis();
        vkb.moveCursor(moveX, moveY);
    }

    if (shortPress) {
        String key = vkb.pressSelectedKey();
        if (key == "\b") {
            if (inputPassword.length() > 0) {
                inputPassword.remove(inputPassword.length() - 1);
                triggerSfx(SFX_BOUNCE);
            }
        } else if (key == "\n") {
            connectToSelectedWifi(targetSsid, inputPassword);
            enterSetting();
            return;
        } else if (key.length() > 0) {
            if (inputPassword.length() < 32) {
                inputPassword += key;
                triggerSfx(SFX_MOVE);
            }
        } else {
            triggerSfx(SFX_MOVE);
        }
    }

    kbCanvas.fillScreen(BLACK);
    kbCanvas.setTextSize(1);
    kbCanvas.setTextColor(CYAN);
    kbCanvas.setCursor(4, 3);
    kbCanvas.printf("WiFi: %s", targetSsid.c_str());

    kbCanvas.drawRect(3, 14, 154, 17, GREEN);
    kbCanvas.setTextColor(WHITE);
    kbCanvas.setCursor(7, 19);
    String dispPass = inputPassword;
    if (dispPass.length() > 23) dispPass = dispPass.substring(dispPass.length() - 23);
    kbCanvas.print(dispPass);
    if ((millis() / 400) % 2 == 0) kbCanvas.print("_");

    vkb.draw();
    tft.drawRGBBitmap(0, 0, kbCanvas.getBuffer(), 160, 128);
}

// =========================================================================
// 15. BẢNG XẾP HẠNG MẠNG NỘI BỘ TOP 10 & KẾT QUẢ
// =========================================================================
void renderLeaderboard() {
    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, CYAN);
    canvasCentered("LAN TOP 10", 6, YELLOW, 1);
    canvas.drawFastHLine(SAFE_X, 18, SAFE_W, CYAN);

    if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(20))) {
        int total = (int)leaderboard.size();
        if (total == 0) {
            canvasCentered("Chua co ky luc", 65, GRAY, 1);
            canvasCentered("Choi game de luu", 82, GREEN, 1);
            canvasCentered("Top 10 mang LAN", 94, CYAN, 1);
        } else {
            int maxOffset = max(0, total - 7);
            lbScrollOffset = constrain(lbScrollOffset, 0, maxOffset);

            for (int i = lbScrollOffset; i < min(total, lbScrollOffset + 7); i++) {
                int row = i - lbScrollOffset;
                int y = 23 + row * 17;
                uint16_t rowCol = (i == 0) ? YELLOW : (i == 1) ? WHITE : (i == 2) ? ORANGE : CYAN;
                canvas.setTextColor(rowCol);
                canvas.setCursor(5, y);
                String shortName = leaderboard[i].player;
                if (shortName.length() > 9) shortName = shortName.substring(0, 9);
                canvas.printf("%2d.%s", i + 1, shortName.c_str());

                canvas.setCursor(82, y);
                canvas.printf("%s:%d", leaderboard[i].game.substring(0, 3).c_str(), leaderboard[i].score);
            }
        }
        xSemaphoreGive(dataMutex);
    }
    canvasCentered("UP/DN:CUON  BTN:BACK", 146, GREEN, 1);
    flushCanvas();
}

void finishGame(bool won) {
    gameWon = won;
    lastGame = state;
    triggerSfx(won ? SFX_WIN : SFX_GAMEOVER);

    int slot = gameSlot(lastGame);
    int finalScore = max(score, scoreP2);
    newRecord = (finalScore > best[slot]);
    if (newRecord) {
        best[slot] = finalScore;
        prefs.begin("minigame", false);
        prefs.putInt(keyFor(lastGame), finalScore);
        prefs.end();
    }

    if (score > 0) {
        recordHighScoreLocalAndLan(gameNames[slot], PLAYER_INFO, score, level[slot]);
    }
    if (twoPlayerMode && scoreP2 > 0) {
        recordHighScoreLocalAndLan(gameNames[slot], p2PlayerName, scoreP2, level[slot]);
    }

    state = RESULT;
    resultChoice = 0;
}

void renderResult() {
    int slot = gameSlot(lastGame);
    canvas.fillScreen(BLACK);
    canvas.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);

    if (twoPlayerMode && (lastGame == SNAKE || lastGame == BREAKOUT)) {
        const char* winner = (score > scoreP2) ? "PLAYER 1 WINS!" : (scoreP2 > score) ? "PLAYER 2 WINS!" : "DRAW GAME!";
        canvasCentered(winner, 14, YELLOW, 1);
    } else {
        canvasCentered(gameWon ? "LEVEL CLEARED!" : "GAME OVER", 14, WHITE, 1);
    }

    canvasCentered(gameNames[slot], 28, gameColors[slot], 2);

    char buf[32];
    if (twoPlayerMode && (lastGame == SNAKE || lastGame == BREAKOUT)) {
        snprintf(buf, sizeof(buf), "P1:%d  -  P2:%d", score, scoreP2);
    } else {
        snprintf(buf, sizeof(buf), "SCORE: %d  LV:%d", score, level[slot]);
    }
    canvasCentered(buf, 52, CYAN, 1);

    if (newRecord) {
        canvasCentered("NEW HIGH SCORE!", 68, YELLOW, 1);
        canvasCentered("Saved to LAN Top 10", 80, GREEN, 1);
    } else {
        snprintf(buf, sizeof(buf), "BEST: %d", best[slot]);
        canvasCentered(buf, 72, GRAY, 1);
    }

    const char* opts[] = {gameWon ? "NEXT LEVEL >" : "PLAY AGAIN >", "BACK TO MENU"};
    for (int i = 0; i < 2; i++) {
        int y = 108 + i * 22;
        uint16_t edge = (i == resultChoice) ? GREEN : GRAY;
        uint16_t fill = (i == resultChoice) ? GREEN : BLACK;
        canvas.fillRoundRect(5, y, 118, 19, 4, fill);
        canvas.drawRoundRect(5, y, 118, 19, 4, edge);
        canvasCentered(opts[i], y + 6, (i == resultChoice) ? BLACK : WHITE, 1);
    }
    flushCanvas();
}

// =========================================================================
// 16. GAME 1: SNAKE
// =========================================================================
constexpr int SC = 8, SX = 4, SY = 28, SN = 15, SMAX = SN * SN;
int sx[SMAX], sy[SMAX], slen = 3, sdx = 1, sdy = 0;
int sx2[SMAX], sy2[SMAX], slen2 = 3, sdx2 = -1, sdy2 = 0;
int fx = 8, fy = 7;
uint32_t snakeTick = 0;
int snakeDelay = 135;

bool snakeOcc(int x, int y) {
    for (int i = 0; i < slen; i++) if (sx[i] == x && sy[i] == y) return true;
    if (twoPlayerMode) {
        for (int i = 0; i < slen2; i++) if (sx2[i] == x && sy2[i] == y) return true;
    }
    return false;
}
void foodSpawn() {
    for (int n = 0; n < 250; n++) {
        int x = random(SN), y = random(SN);
        if (!snakeOcc(x, y)) { fx = x; fy = y; return; }
    }
}
void startSnake() {
    state = SNAKE; score = 0; scoreP2 = 0;
    slen = 3; sdx = 1; sdy = 0;
    snakeDelay = (int)(150.0f / tocDoGame(level[0]));
    sx[0] = 4; sy[0] = 4; sx[1] = 3; sy[1] = 4; sx[2] = 2; sy[2] = 4;
    if (twoPlayerMode) {
        slen2 = 3; sdx2 = -1; sdy2 = 0;
        sx2[0] = 10; sy2[0] = 10; sx2[1] = 11; sy2[1] = 10; sx2[2] = 12; sy2[2] = 10;
    }
    foodSpawn();
    snakeTick = millis();
}
void stepSnake() {
    if (longPress) { saveLevels(); enterMenu(); return; }
    if (joyX < 0 && sdx != 1)      { sdx = -1; sdy = 0; }
    else if (joyX > 0 && sdx != -1){ sdx = 1;  sdy = 0; }
    else if (joyY < 0 && sdy != 1) { sdx = 0;  sdy = -1; }
    else if (joyY > 0 && sdy != -1){ sdx = 0;  sdy = 1; }

    if (twoPlayerMode) {
        if (p2JoyX < 0 && sdx2 != 1)      { sdx2 = -1; sdy2 = 0; }
        else if (p2JoyX > 0 && sdx2 != -1){ sdx2 = 1;  sdy2 = 0; }
        else if (p2JoyY < 0 && sdy2 != 1) { sdx2 = 0;  sdy2 = -1; }
        else if (p2JoyY > 0 && sdy2 != -1){ sdx2 = 0;  sdy2 = 1; }
    }

    uint32_t now = millis();
    if (now - snakeTick >= (uint32_t)snakeDelay) {
        snakeTick = now;
        int nx = sx[0] + sdx, ny = sy[0] + sdy;
        bool eat1 = (nx == fx && ny == fy);

        if (nx < 0 || nx >= SN || ny < 0 || ny >= SN) { finishGame(false); return; }
        for (int i = 0; i < slen - (eat1 ? 0 : 1); i++) {
            if (sx[i] == nx && sy[i] == ny) { finishGame(false); return; }
        }
        if (twoPlayerMode) {
            for (int i = 0; i < slen2; i++) {
                if (sx2[i] == nx && sy2[i] == ny) { scoreP2 += 50; finishGame(false); return; }
            }
        }
        if (eat1) { if (slen < SMAX) slen++; score += 10; triggerSfx(SFX_EAT); }
        for (int i = slen - 1; i > 0; i--) { sx[i] = sx[i - 1]; sy[i] = sy[i - 1]; }
        sx[0] = nx; sy[0] = ny;

        bool eat2 = false;
        if (twoPlayerMode) {
            int nx2 = sx2[0] + sdx2, ny2 = sy2[0] + sdy2;
            eat2 = (nx2 == fx && ny2 == fy);
            if (nx2 < 0 || nx2 >= SN || ny2 < 0 || ny2 >= SN) { score += 50; finishGame(false); return; }
            for (int i = 0; i < slen2 - (eat2 ? 0 : 1); i++) {
                if (sx2[i] == nx2 && sy2[i] == ny2) { score += 50; finishGame(false); return; }
            }
            for (int i = 0; i < slen; i++) {
                if (sx[i] == nx2 && sy[i] == ny2) { score += 50; finishGame(false); return; }
            }
            if (eat2) { if (slen2 < SMAX) slen2++; scoreP2 += 10; triggerSfx(SFX_EAT); }
            for (int i = slen2 - 1; i > 0; i--) { sx2[i] = sx2[i - 1]; sy2[i] = sy2[i - 1]; }
            sx2[0] = nx2; sy2[0] = ny2;
        }
        if (eat1 || eat2) {
            if (slen >= SMAX) { finishGame(true); return; }
            foodSpawn();
        }
    }

    canvas.fillScreen(BLACK);
    drawCanvasHeader("SNAKE", GREEN);
    canvas.drawRect(SX - 1, SY - 1, SN * SC + 1, SN * SC + 1, levelColor(level[0]));
    canvas.fillRect(SX + fx * SC, SY + fy * SC, SC - 1, SC - 1, RED);
    for (int i = 0; i < slen; i++) {
        canvas.fillRect(SX + sx[i] * SC, SY + sy[i] * SC, SC - 1, SC - 1, i ? GREEN : YELLOW);
    }
    if (twoPlayerMode) {
        for (int i = 0; i < slen2; i++) {
            canvas.fillRect(SX + sx2[i] * SC, SY + sy2[i] * SC, SC - 1, SC - 1, i ? CYAN : MAGENTA);
        }
    }
    flushCanvas();
}

// =========================================================================
// 17. GAME 2: TETRIS
// =========================================================================
constexpr int TC = 10, TR = 16, TETRIS_CELL = 8, TETRIS_X0 = 24, TETRIS_Y0 = 25;
uint8_t board[TR][TC];
const uint16_t pieceColor[] = {CYAN, YELLOW, MAGENTA, GREEN, RED, BLUE, ORANGE};
const uint16_t shapes[7][4] = {
    {0x0F00, 0x2222, 0x00F0, 0x4444}, {0x0660, 0x0660, 0x0660, 0x0660},
    {0x0E40, 0x4C40, 0x4E00, 0x4640}, {0x06C0, 0x8C40, 0x06C0, 0x8C40},
    {0x0C60, 0x4C80, 0x0C60, 0x4C80}, {0x08E0, 0x6440, 0x0E20, 0x44C0},
    {0x02E0, 0x4460, 0x0E80, 0xC440}
};
int piece = 0, rot = 0, px = 3, py = 0;
uint32_t tetrisTick = 0;

bool blockAt(int p, int r, int x, int y) {
    return (shapes[p][r & 3] >> (15 - (y * 4 + x))) & 1;
}
bool collide(int x, int y, int r) {
    for (int by = 0; by < 4; by++) {
        for (int bx = 0; bx < 4; bx++) {
            if (blockAt(piece, rot + r, bx, by)) {
                int xx = x + bx, yy = y + by;
                if (xx < 0 || xx >= TC || yy >= TR) return true;
                if (yy >= 0 && board[yy][xx]) return true;
            }
        }
    }
    return false;
}
void spawnPiece() {
    piece = random(7); rot = 0; px = 3; py = 0;
    if (collide(px, py, 0)) finishGame(false);
}
void lockPiece() {
    for (int by = 0; by < 4; by++) {
        for (int bx = 0; bx < 4; bx++) {
            if (blockAt(piece, rot, bx, by)) {
                int yy = py + by, xx = px + bx;
                if (yy >= 0 && yy < TR && xx >= 0 && xx < TC) board[yy][xx] = piece + 1;
            }
        }
    }
    int cleared = 0;
    for (int y = TR - 1; y >= 0;) {
        bool full = true;
        for (int x = 0; x < TC; x++) if (!board[y][x]) { full = false; break; }
        if (full) {
            cleared++;
            for (int yy = y; yy > 0; yy--) for (int x = 0; x < TC; x++) board[yy][x] = board[yy - 1][x];
            for (int x = 0; x < TC; x++) board[0][x] = 0;
        } else y--;
    }
    if (cleared) {
        score += cleared == 1 ? 100 : cleared == 2 ? 300 : cleared == 3 ? 500 : 800;
        triggerSfx(SFX_CLEAR);
    } else {
        triggerSfx(SFX_BOUNCE);
    }
    spawnPiece();
}
void startTetris() {
    state = TETRIS; score = 0;
    memset(board, 0, sizeof(board));
    spawnPiece();
    tetrisTick = millis();
}
void stepTetris() {
    if (longPress) { saveLevels(); enterMenu(); return; }
    if (joyX && millis() - lastInput > 105) {
        lastInput = millis();
        if (!collide(px + joyX, py, 0)) { px += joyX; triggerSfx(SFX_MOVE); }
    }
    if (joyY < 0 && millis() - lastInput > 130) {
        lastInput = millis();
        if (!collide(px, py, 1)) { rot = (rot + 1) & 3; triggerSfx(SFX_MOVE); }
    }
    uint32_t now = millis();
    float tocDo = tocDoGame(level[1]);
    uint32_t interval = max((uint32_t)35, joyY > 0 ? (uint32_t)(65.0f / tocDo) : (uint32_t)(480.0f / tocDo));
    if (now - tetrisTick >= interval) {
        tetrisTick = now;
        if (!collide(px, py + 1, 0)) py++;
        else { lockPiece(); if (state != TETRIS) return; }
    }

    canvas.fillScreen(BLACK);
    drawCanvasHeader("TETRIS", CYAN);
    canvas.drawRect(TETRIS_X0 - 1, TETRIS_Y0 - 1, TC * TETRIS_CELL + 1, TR * TETRIS_CELL + 1, levelColor(level[1]));
    for (int y = 0; y < TR; y++) {
        for (int x = 0; x < TC; x++) {
            if (board[y][x]) {
                canvas.fillRect(TETRIS_X0 + x * TETRIS_CELL, TETRIS_Y0 + y * TETRIS_CELL, TETRIS_CELL - 1, TETRIS_CELL - 1, pieceColor[board[y][x] - 1]);
            }
        }
    }
    for (int by = 0; by < 4; by++) {
        for (int bx = 0; bx < 4; bx++) {
            if (blockAt(piece, rot, bx, by) && py + by >= 0) {
                canvas.fillRect(TETRIS_X0 + (px + bx) * TETRIS_CELL, TETRIS_Y0 + (py + by) * TETRIS_CELL, TETRIS_CELL - 1, TETRIS_CELL - 1, pieceColor[piece]);
            }
        }
    }
    flushCanvas();
}

// =========================================================================
// 18. GAME 3: PAC-MAN
// =========================================================================
constexpr int PC = 15, PR = 14, PAC_CELL = 8, PAC_X0 = 4, PAC_Y0 = 29;
const char mazeSeed[PR][PC + 1] = {
    "###############", "#.............#", "#.###.###.###.#", "#.............#",
    "#.###.#.#.###.#", "#.....#.#.....#", "###.#.....#.###", "#...#.....#...#",
    "#.###.#.#.###.#", "#.............#", "#.###.###.###.#", "#.............#",
    "#.............#", "###############"
};
char maze[PR][PC + 1];
int pacX = 1, pacY = 1, ghostX = 13, ghostY = 12;
uint32_t pacTick = 0, ghostTick = 0;

bool oTrong(int x, int y) {
    return x >= 0 && x < PC && y >= 0 && y < PR && maze[y][x] != '#';
}
void diChuyenGhostBFS() {
    const int SO_O = PC * PR;
    int hangDoi[SO_O], cha[SO_O];
    bool daTham[SO_O] = {false};
    int dau = 0, cuoi = 0;
    int batDau = ghostY * PC + ghostX, dich = pacY * PC + pacX;
    hangDoi[cuoi++] = batDau; daTham[batDau] = true; cha[batDau] = -1;
    const int dx[4] = {1, -1, 0, 0}, dy[4] = {0, 0, 1, -1};

    while (dau < cuoi && !daTham[dich]) {
        int o = hangDoi[dau++], x = o % PC, y = o / PC;
        for (int i = 0; i < 4; i++) {
            int nx = x + dx[i], ny = y + dy[i];
            if (!oTrong(nx, ny)) continue;
            int ke = ny * PC + nx;
            if (daTham[ke]) continue;
            daTham[ke] = true; cha[ke] = o; hangDoi[cuoi++] = ke;
        }
    }
    if (!daTham[dich]) return;
    int buoc = dich;
    while (cha[buoc] != batDau && cha[buoc] != -1) buoc = cha[buoc];
    if (cha[buoc] != -1) { ghostX = buoc % PC; ghostY = buoc / PC; }
}
void startPacman() {
    state = PACMAN; score = 0; scoreP2 = 0;
    memcpy(maze, mazeSeed, sizeof(mazeSeed));
    pacX = 1; pacY = 1; ghostX = 13; ghostY = 12;
    pacTick = millis(); ghostTick = millis();
}
void stepPacman() {
    if (longPress) { saveLevels(); enterMenu(); return; }
    uint32_t now = millis();
    float tocDo = tocDoGame(level[2]);
    uint32_t stepMs = max((uint32_t)18, (uint32_t)(125.0f / tocDo));

    if ((joyX != 0 || joyY != 0) && now - pacTick >= stepMs) {
        pacTick = now;
        int dx = (joyX != 0) ? joyX : 0, dy = (joyX != 0) ? 0 : joyY;
        if (oTrong(pacX + dx, pacY + dy)) {
            pacX += dx; pacY += dy;
            if (maze[pacY][pacX] == '.') {
                maze[pacY][pacX] = ' ';
                score += 10;
                triggerSfx(SFX_EAT);
            }
        }
    }

    if (twoPlayerMode && (millis() - p2LastPing < 5000)) {
        if ((p2JoyX != 0 || p2JoyY != 0) && now - ghostTick >= stepMs + 15) {
            ghostTick = now;
            int gx = (p2JoyX != 0) ? p2JoyX : 0, gy = (p2JoyX != 0) ? 0 : p2JoyY;
            if (oTrong(ghostX + gx, ghostY + gy)) { ghostX += gx; ghostY += gy; }
        }
    } else {
        uint32_t aiMs = max((uint32_t)120, (uint32_t)(500.0f / tocDo));
        if (now - ghostTick >= aiMs) { ghostTick = now; diChuyenGhostBFS(); }
    }

    if (pacX == ghostX && pacY == ghostY) { finishGame(false); return; }

    bool conCham = false;
    for (int y = 0; y < PR && !conCham; y++) {
        for (int x = 0; x < PC; x++) if (maze[y][x] == '.') { conCham = true; break; }
    }
    if (!conCham) { finishGame(true); return; }

    canvas.fillScreen(BLACK);
    drawCanvasHeader("PAC-MAN", YELLOW);
    for (int y = 0; y < PR; y++) {
        for (int x = 0; x < PC; x++) {
            int xx = PAC_X0 + x * PAC_CELL, yy = PAC_Y0 + y * PAC_CELL;
            if (maze[y][x] == '#') {
                canvas.fillRect(xx, yy, PAC_CELL, PAC_CELL, NAVY);
                canvas.drawRect(xx, yy, PAC_CELL, PAC_CELL, BLUE);
            } else if (maze[y][x] == '.') {
                canvas.fillCircle(xx + 4, yy + 4, 1, YELLOW);
            }
        }
    }
    int px0 = PAC_X0 + pacX * PAC_CELL, py0 = PAC_Y0 + pacY * PAC_CELL;
    canvas.fillCircle(px0 + 4, py0 + 4, 3, YELLOW);
    canvas.fillCircle(px0 + 5, py0 + 2, 1, BLACK);

    int gx0 = PAC_X0 + ghostX * PAC_CELL, gy0 = PAC_Y0 + ghostY * PAC_CELL;
    canvas.fillCircle(gx0 + 4, gy0 + 4, 3, RED);
    canvas.fillCircle(gx0 + 3, gy0 + 3, 1, WHITE);
    canvas.fillCircle(gx0 + 6, gy0 + 3, 1, WHITE);
    flushCanvas();
}

// =========================================================================
// 19. GAME 4: BREAKOUT
// =========================================================================
constexpr int BRICK_ROWS = 5, BC = 8, BW = 14, BH = 6, BX0 = 3, PADY = 146, PAD2Y = 26, PADW = 30;
bool bricks[BRICK_ROWS][BC];
int paddle = 49, paddle2 = 49;
float ballx = 64, bally = 105, bvx = 1.5f, bvy = -2.0f;
uint32_t breakTick = 0;

uint16_t brickCol(int r) {
    const uint16_t c[] = {RED, ORANGE, YELLOW, GREEN, CYAN};
    return c[r];
}
void startBreakout() {
    state = BREAKOUT; score = 0; scoreP2 = 0;
    paddle = 49; paddle2 = 49;
    ballx = 64; bally = 105;
    float k = tocDoGame(level[3]);
    bvx = 1.5f * k; bvy = -2.0f * k;
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BC; c++) {
            bricks[r][c] = ((r + c + level[3]) % (1 + (level[3] % 3)) != 0) || (r == 0);
        }
    }
    breakTick = millis();
}
void stepBreakout() {
    if (longPress) { saveLevels(); enterMenu(); return; }
    uint32_t now = millis();
    if (now - breakTick < 16) return;
    breakTick = now;

    paddle = constrain(paddle + joyX * 4, 4, 124 - PADW);
    if (twoPlayerMode) paddle2 = constrain(paddle2 + p2JoyX * 4, 4, 124 - PADW);

    ballx += bvx; bally += bvy;
    if (ballx < 4)   { ballx = 4;   bvx = fabsf(bvx);  triggerSfx(SFX_BOUNCE); }
    if (ballx > 122) { ballx = 122; bvx = -fabsf(bvx); triggerSfx(SFX_BOUNCE); }

    if (bvy > 0 && bally + 2 >= PADY && ballx >= paddle && ballx <= paddle + PADW) {
        bally = PADY - 3; bvy = -fabsf(bvy);
        float hit = (ballx - (paddle + PADW / 2.0f)) / (PADW / 2.0f);
        float tocDo = tocDoGame(level[3]);
        bvx = hit * 2.3f * tocDo;
        if (fabsf(bvx) < 0.3f * tocDo) bvx = bvx < 0 ? -0.3f * tocDo : 0.3f * tocDo;
        triggerSfx(SFX_BOUNCE);
    }

    if (twoPlayerMode) {
        if (bvy < 0 && bally - 2 <= PAD2Y + 5 && ballx >= paddle2 && ballx <= paddle2 + PADW) {
            bally = PAD2Y + 8; bvy = fabsf(bvy);
            float hit = (ballx - (paddle2 + PADW / 2.0f)) / (PADW / 2.0f);
            float tocDo = tocDoGame(level[3]);
            bvx = hit * 2.3f * tocDo;
            triggerSfx(SFX_BOUNCE);
        }
        if (bally < 21) { score += 100; finishGame(true); return; }
        if (bally > H)  { scoreP2 += 100; finishGame(false); return; }
    } else {
        if (bally < 22) { bally = 22; bvy = fabsf(bvy); triggerSfx(SFX_BOUNCE); }
        if (bally > H)  { finishGame(false); return; }
    }

    int baseBrickY = twoPlayerMode ? 58 : 25;
    for (int r = 0; r < BRICK_ROWS; r++) {
        bool hit = false;
        for (int c = 0; c < BC; c++) {
            if (bricks[r][c]) {
                int x = BX0 + c * 15, y = baseBrickY + r * 7;
                if (ballx + 2 >= x && ballx - 2 <= x + BW && bally + 2 >= y && bally - 2 <= y + BH) {
                    bricks[r][c] = false;
                    if (bvy < 0) score += (BRICK_ROWS - r) * 10;
                    else scoreP2 += (BRICK_ROWS - r) * 10;
                    bvy = -bvy;
                    triggerSfx(SFX_EAT);
                    hit = true;
                    break;
                }
            }
        }
        if (hit) break;
    }

    int remain = 0;
    for (int r = 0; r < BRICK_ROWS; r++) for (int c = 0; c < BC; c++) remain += bricks[r][c];
    if (!remain) { finishGame(true); return; }

    canvas.fillScreen(BLACK);
    drawCanvasHeader(twoPlayerMode ? "VS BREAK" : "BREAKOUT", ORANGE);
    for (int r = 0; r < BRICK_ROWS; r++) {
        for (int c = 0; c < BC; c++) {
            if (bricks[r][c]) canvas.fillRect(BX0 + c * 15, baseBrickY + r * 7, BW, BH, brickCol(r));
        }
    }
    canvas.fillRoundRect(paddle, PADY, PADW, 5, 2, CYAN);
    if (twoPlayerMode) canvas.fillRoundRect(paddle2, PAD2Y, PADW, 5, 2, MAGENTA);
    canvas.fillCircle((int)ballx, (int)bally, 2, YELLOW);
    flushCanvas();
}

// =========================================================================
// 20. PPT REMOTE (ESP-NOW)
// =========================================================================
void drawPptStatusDot() {
    uint16_t dotColor = RED;
    if (millis() - lastPacketMs < 3500) {
        if (contentMode == CONTENT_CURR_SLIDE)      dotColor = GREEN;
        else if (contentMode == CONTENT_TEXT)       dotColor = CYAN;
        else if (contentMode == CONTENT_NEXT_SLIDE) dotColor = YELLOW;
    }
    tft.fillRect((orientMode == ORIENT_LANDSCAPE) ? 156 : 124, 1, 3, 3, dotColor);
}

void drawScaledPortraitImage(const uint16_t* srcBuf, int dstX, int dstY) {
    const int dstW = 126, dstH = 78;
    for (int y = 0; y < dstH; y++) {
        int srcY = (y * IMG_H) / dstH;
        for (int x = 0; x < dstW; x++) {
            int srcX = (x * IMG_W) / dstW;
            portScaledBuf[y * dstW + x] = srcBuf[srcY * IMG_W + srcX];
        }
    }
    tft.drawRGBBitmap(dstX, dstY, portScaledBuf, dstW, dstH);
}

void renderVietnameseTextRegion(int x0, int y0, int boxW, int boxH) {
    tft.fillRect(x0, y0, boxW, boxH, BLACK);
    u8g2.setFontMode(1);
    u8g2.setFontDirection(0);
    u8g2.setForegroundColor(WHITE);
    u8g2.setFont(u8g2_font_unifont_t_vietnamese1);

    const int maxWidth = boxW - 4, lineHeight = 13;
    const int maxVisibleLines = max(1, (boxH - 2) / lineHeight);
    String text = String(slideTextBuf);
    if (text.length() == 0) text = "(Chờ dữ liệu slide...)";

    std::vector<String> lines;
    int start = 0;
    while (start < (int)text.length()) {
        int nlPos = text.indexOf('\n', start);
        String paragraph = (nlPos == -1) ? text.substring(start) : text.substring(start, nlPos);
        start = (nlPos == -1) ? text.length() : nlPos + 1;
        paragraph.trim();
        if (paragraph.length() == 0) continue;

        String currentLine = "";
        int wordStart = 0;
        while (wordStart < (int)paragraph.length()) {
            int spacePos = paragraph.indexOf(' ', wordStart);
            String word = (spacePos == -1) ? paragraph.substring(wordStart) : paragraph.substring(wordStart, spacePos);
            wordStart = (spacePos == -1) ? paragraph.length() : spacePos + 1;
            String testLine = (currentLine.length() == 0) ? word : (currentLine + " " + word);
            if (u8g2.getUTF8Width(testLine.c_str()) <= maxWidth) currentLine = testLine;
            else { if (currentLine.length() > 0) lines.push_back(currentLine); currentLine = word; }
        }
        if (currentLine.length() > 0) lines.push_back(currentLine);
    }

    totalWrappedLines = (int)lines.size();
    int maxScroll = max(0, totalWrappedLines - maxVisibleLines);
    textScrollLine = constrain(textScrollLine, 0, maxScroll);

    int yCursor = y0 + 12;
    for (int i = textScrollLine; i < min(totalWrappedLines, textScrollLine + maxVisibleLines); i++) {
        u8g2.setCursor(x0 + 2, yCursor);
        u8g2.print(lines[i]);
        yCursor += lineHeight;
    }
}

void renderPptFullScreen() {
    tft.fillScreen(BLACK);
    if (orientMode == ORIENT_LANDSCAPE) {
        if (contentMode == CONTENT_CURR_SLIDE) tft.drawRGBBitmap(1, 1, currImgBuffer, IMG_W, IMG_H);
        else if (contentMode == CONTENT_NEXT_SLIDE) tft.drawRGBBitmap(1, 1, nextImgBuffer, IMG_W, IMG_H);
        else renderVietnameseTextRegion(1, 1, 158, 126);
    } else {
        drawScaledPortraitImage(currImgBuffer, 1, 1);
        tft.drawFastHLine(1, 80, 126, 0x4208);
        if (contentMode == CONTENT_NEXT_SLIDE) drawScaledPortraitImage(nextImgBuffer, 1, 81);
        else renderVietnameseTextRegion(1, 81, 126, 78);
    }
    drawPptStatusDot();
}

void startPptRemote() {
    state = PPT_REMOTE;
    orientMode = ORIENT_LANDSCAPE;
    contentMode = CONTENT_CURR_SLIDE;
    textScrollLine = 0;
    tft.setRotation(1);
    renderPptFullScreen();
}

void stepPptRemote() {
    static unsigned long lastMoveMs = 0, btnDownMs = 0, firstReleaseMs = 0, lastDotMs = 0;
    static bool btnPressed = false, rotateHandled = false;
    static uint8_t clickCount = 0;

    bool rawBtn = (digitalRead(JOY_SW_PIN) == LOW);
    if (rawBtn && !btnPressed) {
        btnPressed = true; rotateHandled = false; btnDownMs = millis();
    } else if (rawBtn && btnPressed) {
        unsigned long holdDuration = millis() - btnDownMs;
        if (holdDuration >= 1500) {
            btnPressed = false; clickCount = 0;
            enterMenu();
            return;
        }
        if (holdDuration >= 550 && !rotateHandled) {
            rotateHandled = true; clickCount = 0;
            if (orientMode == ORIENT_LANDSCAPE) {
                orientMode = ORIENT_PORTRAIT; tft.setRotation(0); contentMode = CONTENT_TEXT; textScrollLine = 0;
            } else {
                orientMode = ORIENT_LANDSCAPE; tft.setRotation(1); contentMode = CONTENT_CURR_SLIDE;
            }
            renderPptFullScreen();
        }
    } else if (!rawBtn && btnPressed) {
        btnPressed = false;
        if (!rotateHandled && (millis() - btnDownMs > 20)) {
            clickCount++;
            if (clickCount == 1) firstReleaseMs = millis();
            else if (clickCount == 2) {
                clickCount = 0;
                if (orientMode == ORIENT_LANDSCAPE) contentMode = (contentMode == CONTENT_NEXT_SLIDE) ? CONTENT_CURR_SLIDE : CONTENT_NEXT_SLIDE;
                else contentMode = CONTENT_NEXT_SLIDE;
                renderPptFullScreen();
            }
        }
    }

    if (clickCount == 1 && (millis() - firstReleaseMs > 320)) {
        clickCount = 0;
        if (orientMode == ORIENT_LANDSCAPE) {
            if (contentMode == CONTENT_CURR_SLIDE) { contentMode = CONTENT_TEXT; textScrollLine = 0; }
            else contentMode = CONTENT_CURR_SLIDE;
        } else {
            contentMode = CONTENT_TEXT; textScrollLine = 0;
        }
        renderPptFullScreen();
    }

    if (millis() - lastMoveMs >= 240) {
        int rawX = analogRead(JOY_X_PIN), rawY = analogRead(JOY_Y_PIN);
        int navAxis    = (orientMode == ORIENT_LANDSCAPE) ? rawY : rawX;
        int scrollAxis = (orientMode == ORIENT_LANDSCAPE) ? rawX : (4095 - rawY);

        if (navAxis < 700) { lastMoveMs = millis(); sendPptCommand(2); triggerSfx(SFX_MOVE); }
        else if (navAxis > 3400) { lastMoveMs = millis(); sendPptCommand(1); triggerSfx(SFX_MOVE); }
        else if (contentMode == CONTENT_TEXT) {
            int visibleLines = (orientMode == ORIENT_LANDSCAPE) ? 9 : 5;
            if (scrollAxis < 700 && textScrollLine > 0) {
                lastMoveMs = millis(); textScrollLine--; renderPptFullScreen();
            } else if (scrollAxis > 3400 && textScrollLine < max(0, totalWrappedLines - visibleLines)) {
                lastMoveMs = millis(); textScrollLine++; renderPptFullScreen();
            }
        }
    }

    if (currImgDirty) {
        currImgDirty = false;
        if (orientMode == ORIENT_PORTRAIT || contentMode == CONTENT_CURR_SLIDE) renderPptFullScreen();
    }
    if (nextImgDirty) {
        nextImgDirty = false;
        if (contentMode == CONTENT_NEXT_SLIDE) renderPptFullScreen();
    }
    if (textUpdated) {
        textUpdated = false; textScrollLine = 0;
        if (contentMode == CONTENT_TEXT) renderPptFullScreen();
    }
    if (millis() - lastDotMs > 200) { lastDotMs = millis(); drawPptStatusDot(); }
}

// =========================================================================
// 21. SETUP & LOOP (CORE 1)
// =========================================================================
void startGameByIndex(int idx) {
    switch (idx) {
        case 0: startSnake();     break;
        case 1: startTetris();    break;
        case 2: startPacman();    break;
        case 3: startBreakout();  break;
        case 4: startPptRemote(); break;
        case 5: enterSetting();   break;
        default: break;
    }
}

void setup() {
    Serial.begin(115200);
    dataMutex  = xSemaphoreCreateMutex();
    frameMutex = xSemaphoreCreateMutex();

    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    analogReadResolution(12);
    analogSetPinAttenuation(JOY_X_PIN, ADC_11db);
    analogSetPinAttenuation(JOY_Y_PIN, ADC_11db);

    ledcAttach(BUZZER_PIN, 2000, 8);
    // Nếu module còi kêu liên tục khi không phát nhạc, hãy bỏ dấu // ở dòng dưới:
    // ledcOutputInvert(BUZZER_PIN, true);

    SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    tft.setSPISpeed(SPI_CLOCK);
    tft.fillScreen(BLACK);

    u8g2.begin(tft);
    memset(currImgBuffer, 0, sizeof(currImgBuffer));
    memset(nextImgBuffer, 0, sizeof(nextImgBuffer));
    memset(webMirrorBuf, 0, sizeof(webMirrorBuf));
    strcpy(slideTextBuf, "Đang kết nối máy tính...");

    randomSeed((uint32_t)analogRead(JOY_X_PIN) ^ micros());
    calibrate();
    loadBest();
    loadSoundSettings();
    loadWifiCreds();
    loadLeaderboardNvs();
    rawSW = stableSW = (digitalRead(JOY_SW_PIN) == LOW);
    rawChanged = millis();

    xTaskCreatePinnedToCore(networkAudioTask, "NetAudioCore0", 8192, NULL, 1, NULL, 0);

    enterMenu();
    triggerSfx(SFX_WIN);
}

void loop() {
    if (state == PPT_REMOTE) {
        stepPptRemote();
        delay(5);
        return;
    }

    updateInput();

    if (state == MENU) {
        if (joyY && millis() - lastInput > 160) {
            lastInput = millis();
            menuIndex = (menuIndex + (joyY > 0 ? 1 : MENU_COUNT - 1)) % MENU_COUNT;
        }
        if (shortPress) {
            startGameByIndex(menuIndex);
        }
        if (state == MENU) renderMenu();
    }
    else if (state == SETTING) {
        if (longPress) {
            enterMenu();
        } else {
            if (joyY && millis() - lastInput > 160) {
                lastInput = millis();
                settingIndex = (settingIndex + (joyY > 0 ? 1 : SETTING_COUNT - 1)) % SETTING_COUNT;
            }
            if (shortPress) {
                if (settingIndex == 0) {
                    resetPressCount = 0;
                    startWifiScan();
                } else if (settingIndex == 1) {
                    resetPressCount = 0;
                    enterSoundMenu();
                } else if (settingIndex == 2) {
                    resetPressCount = 0;
                    twoPlayerMode = !twoPlayerMode;
                    triggerSfx(SFX_EAT);
                } else if (settingIndex == 3) {
                    resetPressCount = 0;
                    lbScrollOffset = 0;
                    state = LEADERBOARD;
                } else if (settingIndex == 4) {
                    uint32_t now = millis();
                    if (now - resetPressAt > 3000) resetPressCount = 0;
                    resetPressAt = now;
                    resetPressCount++;
                    triggerSfx(SFX_MOVE);
                    if (resetPressCount >= 5) {
                        prefs.begin("minigame", false);
                        prefs.clear();
                        prefs.end();
                        prefs.begin("lan_top10", false);
                        prefs.clear();
                        prefs.end();
                        if (xSemaphoreTake(dataMutex, pdMS_TO_TICKS(50))) {
                            leaderboard.clear();
                            xSemaphoreGive(dataMutex);
                        }
                        for (int i = 0; i < 4; i++) { best[i] = 0; level[i] = 1; }
                        resetPressCount = 0;
                        triggerSfx(SFX_GAMEOVER);
                    }
                }
            }
            if (state == SETTING) renderSetting();
        }
    }
    else if (state == SOUND_MENU) {
        stepSoundMenu();
    }
    else if (state == WIFI_SCAN) {
        stepWifiScan();
    }
    else if (state == WIFI_PASS_INPUT) {
        stepWifiPassInput();
    }
    else if (state == LEADERBOARD) {
        if (joyY && millis() - lastInput > 150) {
            lastInput = millis();
            lbScrollOffset += (joyY > 0) ? 1 : -1;
        }
        renderLeaderboard();
        if (shortPress || longPress) {
            triggerSfx(SFX_MOVE);
            enterSetting();
        }
    }
    else if (state == RESULT) {
        if (joyY && millis() - lastInput > 170) {
            lastInput = millis();
            resultChoice = 1 - resultChoice;
        }
        if (shortPress) {
            if (resultChoice == 0) {
                int slot = gameSlot(lastGame);
                if (gameWon && level[slot] < MAX_LEVEL) level[slot]++;
                saveLevels();
                startGameByIndex(slot);
            } else {
                saveLevels();
                enterMenu();
            }
        }
        if (longPress) {
            saveLevels();
            enterMenu();
        }
        if (state == RESULT) renderResult();
    }
    else {
        switch (state) {
            case SNAKE:    stepSnake();    break;
            case TETRIS:   stepTetris();   break;
            case PACMAN:   stepPacman();   break;
            case BREAKOUT: stepBreakout(); break;
            default: break;
        }
    }

    delay(16);
}