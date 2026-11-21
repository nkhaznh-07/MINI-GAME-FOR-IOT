#include <Arduino.h>
#include <SPI.h>
#include <WiFi.h>
#include <WebServer.h>
#include <esp_now.h>
#include <esp_wifi.h>
#include <Preferences.h>
#include <vector>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
#include <U8g2_for_Adafruit_GFX.h>
#include <VirtualKeyboard.h>

// =========================================================================
// [MODULE ĐIỀU KHIỂN ĐỘC LẬP] - DỄ DÀNG CHUYỂN ĐỔI JOYSTICK <-> 5 NÚT BẤM
// =========================================================================
#define USE_BUTTON_PAD 0

constexpr int JOY_X_PIN  = 4;
constexpr int JOY_Y_PIN  = 5;
constexpr int JOY_SW_PIN = 6;

constexpr int BTN_UP_PIN    = 4;
constexpr int BTN_DOWN_PIN  = 5;
constexpr int BTN_LEFT_PIN  = 15;
constexpr int BTN_RIGHT_PIN = 16;
constexpr int BTN_OK_PIN    = 6;

struct ControlSignal {
  int moveX;
  int moveY;
  bool shortClick;
  bool longHold;
  bool isPressed;
};

class InputControllerModule {
private:
  int centerX = 2048, centerY = 2048;
  bool rawSW = false, stableSW = false, longFired = false;
  uint32_t rawChanged = 0, pressAt = 0;
  static constexpr int DEADZONE = 650;
  static constexpr uint32_t DEBOUNCE_MS = 28;
  static constexpr uint32_t LONG_PRESS_MS = 1000;

  int readAxis(int pin, int center) {
    int d = analogRead(pin) - center;
    return abs(d) < DEADZONE ? 0 : (d < 0 ? -1 : 1);
  }

public:
  void begin() {
#if USE_BUTTON_PAD == 0
    pinMode(JOY_SW_PIN, INPUT_PULLUP);
    analogReadResolution(12);
    analogSetPinAttenuation(JOY_X_PIN, ADC_11db);
    analogSetPinAttenuation(JOY_Y_PIN, ADC_11db);
    calibrateCenter();
#else
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_LEFT_PIN, INPUT_PULLUP);
    pinMode(BTN_RIGHT_PIN, INPUT_PULLUP);
    pinMode(BTN_OK_PIN, INPUT_PULLUP);
#endif
    rawSW = stableSW = readRawSelectButton();
    rawChanged = millis();
  }

  void calibrateCenter() {
#if USE_BUTTON_PAD == 0
    long sx = 0, sy = 0;
    for (int i = 0; i < 35; i++) {
      sx += analogRead(JOY_X_PIN);
      sy += analogRead(JOY_Y_PIN);
      delay(3);
    }
    centerX = sx / 35;
    centerY = sy / 35;
#endif
  }

  bool readRawSelectButton() {
#if USE_BUTTON_PAD == 0
    return (digitalRead(JOY_SW_PIN) == LOW);
#else
    return (digitalRead(BTN_OK_PIN) == LOW);
#endif
  }

  int readRawX() {
#if USE_BUTTON_PAD == 0
    return analogRead(JOY_X_PIN);
#else
    if (digitalRead(BTN_LEFT_PIN) == LOW) return 0;
    if (digitalRead(BTN_RIGHT_PIN) == LOW) return 4095;
    return 2048;
#endif
  }

  int readRawY() {
#if USE_BUTTON_PAD == 0
    return analogRead(JOY_Y_PIN);
#else
    if (digitalRead(BTN_UP_PIN) == LOW) return 0;
    if (digitalRead(BTN_DOWN_PIN) == LOW) return 4095;
    return 2048;
#endif
  }

  ControlSignal poll() {
    uint32_t now = millis();
    ControlSignal sig = {0, 0, false, false, false};

#if USE_BUTTON_PAD == 0
    sig.moveX = readAxis(JOY_X_PIN, centerX);
    sig.moveY = readAxis(JOY_Y_PIN, centerY);
#else
    if (digitalRead(BTN_LEFT_PIN) == LOW) sig.moveX = -1;
    else if (digitalRead(BTN_RIGHT_PIN) == LOW) sig.moveX = 1;
    if (digitalRead(BTN_UP_PIN) == LOW) sig.moveY = -1;
    else if (digitalRead(BTN_DOWN_PIN) == LOW) sig.moveY = 1;
#endif

    bool r = readRawSelectButton();
    if (r != rawSW) {
      rawSW = r;
      rawChanged = now;
    }
    if (rawSW != stableSW && now - rawChanged >= DEBOUNCE_MS) {
      stableSW = rawSW;
      if (stableSW) {
        pressAt = now;
        longFired = false;
      } else {
        if (!longFired && now - pressAt < LONG_PRESS_MS) {
          sig.shortClick = true;
        }
      }
    }
    if (stableSW && !longFired && now - pressAt >= LONG_PRESS_MS) {
      longFired = true;
      sig.longHold = true;
    }
    sig.isPressed = stableSW;
    return sig;
  }
};

InputControllerModule hwControl;

// =========================================================================
// CẤU HÌNH MÀN HÌNH TFT & BUZZER
// =========================================================================
constexpr int TFT_CS_PIN = 10, TFT_DC_PIN = 9, TFT_RST_PIN = 8;
constexpr int TFT_MOSI_PIN = 11, TFT_SCLK_PIN = 12;
constexpr int BUZZER_PIN = 7;

constexpr int W = 128, H = 160;
constexpr uint32_t SPI_CLOCK = 40000000UL;
constexpr int SAFE_X = 2, SAFE_Y = 2, SAFE_W = 124, SAFE_H = 156;

uint8_t c3PeerMac[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
#define WIFI_CHANNEL 1
#define MAX_PAYLOAD 180
#define IMG_W 158
#define IMG_H 126

Adafruit_ST7735 tft(TFT_CS_PIN, TFT_DC_PIN, TFT_RST_PIN);
U8G2_FOR_ADAFRUIT_GFX u8g2;
VirtualKeyboard vkb;
Preferences prefs;
WebServer server(80);
bool webServerRunning = false;

constexpr uint16_t BLACK = ST77XX_BLACK, WHITE = ST77XX_WHITE, RED = ST77XX_RED;
constexpr uint16_t GREEN = ST77XX_GREEN, BLUE = ST77XX_BLUE, YELLOW = ST77XX_YELLOW;
constexpr uint16_t CYAN = ST77XX_CYAN, MAGENTA = ST77XX_MAGENTA, ORANGE = 0xFD20;
constexpr uint16_t GRAY = 0x8410, NAVY = 0x000F, PINK = 0xF81F;

// =========================================================================
// TRẠNG THÁI HỆ THỐNG & CÀI ĐẶT
// =========================================================================
enum State {
  MENU, SNAKE, TETRIS, PACMAN, BREAKOUT, RESULT,
  POWERPOINT,
  SETTING, SET_WIFI, SET_WIFI_SCAN, SET_SOUND
};

State state = MENU, lastGame = MENU;

const char* gameNames[] = {"SNAKE", "TETRIS", "PAC-MAN", "BREAKOUT"};
const uint16_t gameColors[] = {GREEN, CYAN, YELLOW, ORANGE};
int menuIndex = 0, resultChoice = 0;
int settingIndex = 0, wifiMenuIndex = 0, soundMenuIndex = 0;
int resetPressCount = 0;
uint32_t resetPressAt = 0;
bool gameWon = false;
int score = 0, best[4] = {0, 0, 0, 0};
constexpr int MAX_LEVEL = 10;
int level[4] = {1, 1, 1, 1};
bool newRecord = false;

// Cấu hình Sound (GPIO 7)
bool bgmEnabled = true;
int soundVolume = 5;
uint32_t lastBgmNoteMs = 0;
int bgmNoteIdx = 0;
uint32_t sfxStopAtMs = 0;
bool sfxActive = false;

// Cấu hình Wi-Fi & AP
enum WifiSubMode { WIFI_OFF_MODE, WIFI_STA_MODE, WIFI_AP_MODE };
WifiSubMode currentWifiSubMode = WIFI_OFF_MODE;
String savedSsid = "";
String savedPass = "";
const char* apSsid = "MiniGameConsole_V3";
const char* apPass = "12345678";
std::vector<String> scannedSsids;
int scanSelectedIdx = 0;
constexpr int WIFI_VISIBLE_ROWS = 6;
int wifiScrollOffset = 0;

// Input tổng hợp
uint32_t lastInput = 0;
int joyX = 0, joyY = 0;
bool shortPress = false, longPress = false;

int webJoyX[2] = {0, 0};
int webJoyY[2] = {0, 0};
bool webOkPressed[2] = {false, false};
uint32_t webLastSeen[2] = {0, 0};

// =========================================================================
// CẤU TRÚC GÓI TIN POWERPOINT
// =========================================================================
typedef struct __attribute__((packed)) {
  uint16_t magic;      // 0x5A77
  uint8_t type;        // 1: Slide hiện tại, 2: Văn bản UTF-8, 3: Ping, 4: Slide kế tiếp
  uint16_t param1;
  uint16_t param2;
  uint8_t len;
  uint8_t data[MAX_PAYLOAD];
} DataPacket;

typedef struct __attribute__((packed)) {
  uint16_t magic;      // 0x5A78
  uint8_t cmd;         // 1: NEXT, 2: PREV, 3: PONG (Báo trạng thái S3 Online về C3)
  uint8_t checksum;
} CmdPacket;

uint16_t currImgBuffer[IMG_W * IMG_H];
uint16_t nextImgBuffer[IMG_W * IMG_H];
uint16_t portScaledBuf[126 * 78];
char slideTextBuf[2048];
char rxTextBuf[2048];
uint16_t rxExpectedLen = 0;
uint16_t rxReceivedBytes = 0;
volatile bool textUpdated = false;
volatile bool currImgDirty = false;
volatile bool nextImgDirty = false;

enum OrientationMode { ORIENT_LANDSCAPE, ORIENT_PORTRAIT };
enum ContentMode { CONTENT_CURR_SLIDE, CONTENT_TEXT, CONTENT_NEXT_SLIDE };
OrientationMode orientMode = ORIENT_LANDSCAPE;
ContentMode contentMode = CONTENT_CURR_SLIDE;

volatile unsigned long lastPacketMs = 0;
int textScrollLine = 0;
int totalWrappedLines = 0;
bool espNowInitialized = false;
portMUX_TYPE mux = portMUX_INITIALIZER_UNLOCKED;

void drawMenu();
void drawSettingMenu();
void drawWifiMenu();
void drawSoundMenu();
void startPowerPoint();
void stopPowerPoint();
void startGame(int idx);
void startWebServer();
void stopWebServer();

// =========================================================================
// CHUẨN HÓA CHUỖI WI-FI TIẾNG VIỆT
// =========================================================================
String sanitizeWifiLabel(const String& src) {
  String s = src;
  const char* combiningMarks[] = {
    "\xCC\x80", "\xCC\x81", "\xCC\x83", "\xCC\x89", "\xCC\xA3",
    "\xCC\x82", "\xCC\x86", "\xCC\x9B"
  };
  for (const char* m : combiningMarks) s.replace(m, "");

  struct MapPair { const char* utf8; const char* ascii; };
  static const MapPair tbl[] = {
    {"á","a"},{"à","a"},{"ả","a"},{"ã","a"},{"ạ","a"},
    {"ă","a"},{"ắ","a"},{"ằ","a"},{"ẳ","a"},{"ẵ","a"},{"ặ","a"},
    {"â","a"},{"ấ","a"},{"ầ","a"},{"ẩ","a"},{"ẫ","a"},{"ậ","a"},
    {"Á","A"},{"À","A"},{"Ả","A"},{"Ã","A"},{"Ạ","A"},
    {"Ă","A"},{"Ắ","A"},{"Ằ","A"},{"Ẳ","A"},{"Ẵ","A"},{"Ặ","A"},
    {"Â","A"},{"Ấ","A"},{"Ầ","A"},{"Ẩ","A"},{"Ẫ","A"},{"Ậ","A"},
    {"đ","d"},{"Đ","D"},
    {"é","e"},{"è","e"},{"ẻ","e"},{"ẽ","e"},{"ẹ","e"},
    {"ê","e"},{"ế","e"},{"ề","e"},{"ể","e"},{"ễ","e"},{"ệ","e"},
    {"É","E"},{"È","E"},{"Ẻ","E"},{"Ẽ","E"},{"Ẹ","E"},
    {"Ê","E"},{"Ế","E"},{"Ề","E"},{"Ể","E"},{"Ễ","E"},{"Ệ","E"},
    {"í","i"},{"ì","i"},{"ỉ","i"},{"ĩ","i"},{"ị","i"},
    {"Í","I"},{"Ì","I"},{"Ỉ","I"},{"Ĩ","I"},{"Ị","I"},
    {"ó","o"},{"ò","o"},{"ỏ","o"},{"õ","o"},{"ọ","o"},
    {"ô","o"},{"ố","o"},{"ồ","o"},{"ổ","o"},{"ỗ","o"},{"ộ","o"},
    {"ơ","o"},{"ớ","o"},{"ờ","o"},{"ở","o"},{"ỡ","o"},{"ợ","o"},
    {"Ó","O"},{"Ò","O"},{"Ỏ","O"},{"Õ","O"},{"Ọ","O"},
    {"Ô","O"},{"Ố","O"},{"Ồ","O"},{"Ổ","O"},{"Ỗ","O"},{"Ộ","O"},
    {"Ơ","O"},{"Ớ","O"},{"Ờ","O"},{"Ở","O"},{"Ỡ","O"},{"Ợ","O"},
    {"ú","u"},{"ù","u"},{"ủ","u"},{"ũ","u"},{"ụ","u"},
    {"ư","u"},{"ứ","u"},{"ừ","u"},{"ử","u"},{"ữ","u"},{"ự","u"},
    {"Ú","U"},{"Ù","U"},{"Ủ","U"},{"Ũ","U"},{"Ụ","U"},
    {"Ư","U"},{"Ứ","U"},{"Ừ","U"},{"Ử","U"},{"Ữ","U"},{"Ự","U"},
    {"ý","y"},{"ỳ","y"},{"ỷ","y"},{"ỹ","y"},{"ỵ","y"},
    {"Ý","Y"},{"Ỳ","Y"},{"Ỷ","Y"},{"Ỹ","Y"},{"Ỵ","Y"}
  };
  for (const auto& item : tbl) {
    s.replace(item.utf8, item.ascii);
  }

  String clean = "";
  for (int i = 0; i < (int)s.length(); i++) {
    uint8_t c = (uint8_t)s[i];
    if (c >= 32 && c <= 126) clean += (char)c;
  }
  if (clean.length() == 0) clean = "(Hidden/Emoji WiFi)";
  return clean;
}

// =========================================================================
// HỆ THỐNG ÂM THANH BUZZER KHÔNG CHẶN (GPIO 7)
// =========================================================================
void initSound() {
  ledcAttach(BUZZER_PIN, 2000, 8);
  ledcWrite(BUZZER_PIN, 0);
}

void triggerNonBlockingTone(uint32_t freq, uint32_t durationMs) {
  if (state == POWERPOINT || soundVolume <= 0 || freq == 0) return;
  uint8_t duty = map(soundVolume, 1, 10, 25, 128);
  ledcWriteTone(BUZZER_PIN, freq);
  ledcWrite(BUZZER_PIN, duty);
  sfxActive = true;
  sfxStopAtMs = millis() + durationMs;
}

void playTone(uint32_t freq, uint32_t durationMs) {
  if (state == POWERPOINT || soundVolume <= 0 || freq == 0) {
    ledcWriteTone(BUZZER_PIN, 0);
    ledcWrite(BUZZER_PIN, 0);
    return;
  }
  uint8_t duty = map(soundVolume, 1, 10, 25, 128);
  ledcWriteTone(BUZZER_PIN, freq);
  ledcWrite(BUZZER_PIN, duty);
  if (durationMs > 0) {
    delay(durationMs);
    ledcWriteTone(BUZZER_PIN, 0);
    ledcWrite(BUZZER_PIN, 0);
  }
}

void playClickSfx() {
  triggerNonBlockingTone(1350, 25);
}

void playEatSfx() {
  if (state == POWERPOINT || soundVolume <= 0) return;
  playTone(880, 22);
  playTone(1320, 30);
}

void playGameOverSfx() {
  if (state == POWERPOINT || soundVolume <= 0) return;
  playTone(440, 85);
  playTone(349, 85);
  playTone(261, 160);
}

void playWinSfx() {
  if (state == POWERPOINT || soundVolume <= 0) return;
  playTone(523, 65);
  playTone(659, 65);
  playTone(784, 65);
  playTone(1046, 140);
}

static const uint16_t bgmSnake[16]    = {392, 0, 440, 0, 523, 0, 440, 0, 392, 0, 330, 0, 294, 0, 330, 0};
static const uint16_t bgmTetris[16]   = {659, 0, 494, 523, 587, 0, 523, 494, 440, 0, 440, 523, 659, 0, 587, 523};
static const uint16_t bgmPacman[16]   = {494, 0, 988, 0, 740, 0, 622, 0, 988, 740, 0, 622, 0, 523, 0, 0};
static const uint16_t bgmBreakout[16] = {523, 0, 523, 587, 659, 0, 659, 587, 523, 0, 494, 440, 440, 0, 494, 523};

void stopBgm() {
  sfxActive = false;
  ledcWriteTone(BUZZER_PIN, 0);
  ledcWrite(BUZZER_PIN, 0);
}

void updateBgm() {
  if (state == POWERPOINT) {
    if (sfxActive) stopBgm();
    return;
  }

  uint32_t now = millis();

  if (sfxActive) {
    if (now >= sfxStopAtMs) {
      sfxActive = false;
      ledcWriteTone(BUZZER_PIN, 0);
      ledcWrite(BUZZER_PIN, 0);
    } else {
      return;
    }
  }

  if (!bgmEnabled || soundVolume <= 0) return;

  const uint16_t* activeMelody = nullptr;
  uint32_t tempoMs = 210;
  if (state == SNAKE)         { activeMelody = bgmSnake;    tempoMs = 220; }
  else if (state == TETRIS)   { activeMelody = bgmTetris;   tempoMs = 210; }
  else if (state == PACMAN)   { activeMelody = bgmPacman;   tempoMs = 195; }
  else if (state == BREAKOUT) { activeMelody = bgmBreakout; tempoMs = 230; }
  else return;

  if (now - lastBgmNoteMs >= tempoMs) {
    lastBgmNoteMs = now;
    uint16_t freq = activeMelody[bgmNoteIdx];
    bgmNoteIdx = (bgmNoteIdx + 1) % 16;
    if (freq > 0) {
      uint8_t duty = map(soundVolume, 1, 10, 6, 45);
      ledcWriteTone(BUZZER_PIN, freq);
      ledcWrite(BUZZER_PIN, duty);
    } else {
      ledcWriteTone(BUZZER_PIN, 0);
      ledcWrite(BUZZER_PIN, 0);
    }
  }
}

// =========================================================================
// CẬP NHẬT INPUT HỆ THỐNG
// =========================================================================
void updateInput() {
  static int prevJoyX = 0;
  static int prevJoyY = 0;
  static uint32_t lastJoyBeepMs = 0;
  uint32_t now = millis();

  ControlSignal hw = hwControl.poll();

  joyX = (hw.moveX != 0) ? hw.moveX : webJoyX[0];
  joyY = (hw.moveY != 0) ? hw.moveY : webJoyY[0];

  if (state != SNAKE) {
    if (webJoyX[1] != 0) joyX = webJoyX[1];
    if (webJoyY[1] != 0) joyY = webJoyY[1];
  }

  if (state != POWERPOINT) {
    bool justMoved = ((joyX != 0 && prevJoyX == 0) || (joyY != 0 && prevJoyY == 0));
    bool stillHolding = ((joyX != 0 || joyY != 0) && (now - lastJoyBeepMs >= 150));

    if (justMoved || stillHolding) {
      lastJoyBeepMs = now;
      playClickSfx();
    }
  }

  prevJoyX = joyX;
  prevJoyY = joyY;

  shortPress = hw.shortClick;
  longPress = hw.longHold;

  for (int p = 0; p < 2; p++) {
    if (webOkPressed[p]) {
      webOkPressed[p] = false;
      shortPress = true;
    }
  }
}

// =========================================================================
// WEB GAME SERVER (ĐÃ FIX LỖI CÚ PHÁP TEMPLATE LITERAL JS)
// =========================================================================
const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="vi">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1.0, user-scalable=no">
<title>Mini Game 2P Pad</title>
<style>
  * { box-sizing: border-box; user-select: none; -webkit-user-select: none; touch-action: manipulation; }
  body {
    margin: 0; background: #090d16; color: #fff; font-family: sans-serif;
    display: flex; flex-direction: column; justify-content: space-between;
    align-items: center; height: 100vh; padding: 14px; overflow: hidden;
  }
  .top-bar { display: flex; gap: 6px; flex-wrap: wrap; justify-content: center; width: 100%; }
  select, button.gbtn {
    background: #1e293b; color: #f8fafc; border: 1px solid #475569;
    padding: 8px 11px; border-radius: 10px; font-size: 13px; font-weight: bold;
  }
  button.gbtn:active { background: #0284c7; }
  .pad-container {
    display: flex; justify-content: space-between; align-items: center;
    width: 100%; max-width: 440px; flex: 1; padding: 10px 12px;
  }
  .ok-btn {
    width: 96px; height: 96px; border-radius: 50%;
    background: #14532d; border: 3px solid #22c55e;
    box-shadow: 0 0 18px rgba(34, 197, 94, 0.25);
    cursor: pointer;
  }
  .ok-btn:active { background: #22c55e; transform: scale(0.93); }

  .dpad {
    display: grid;
    grid-template-columns: 64px 64px 64px;
    grid-template-rows: 64px 64px 64px;
    gap: 6px;
  }
  .btn {
    background: #1e293b; border: 2px solid #38bdf8; color: #fff;
    font-size: 22px; font-weight: bold; border-radius: 16px;
    display: flex; align-items: center; justify-content: center; cursor: pointer;
  }
  .btn:active, .btn.active { background: #38bdf8; color: #000; transform: scale(0.94); }
</style>
</head>
<body>
  <div class="top-bar">
    <select id="playerSel">
      <option value="0">Player 1</option>
      <option value="1">Player 2</option>
    </select>
    <button class="gbtn" onclick="cmd('start',0)">Snake</button>
    <button class="gbtn" onclick="cmd('start',1)">Tetris</button>
    <button class="gbtn" onclick="cmd('start',2)">Pac-Man</button>
    <button class="gbtn" onclick="cmd('start',3)">Breakout</button>
    <button class="gbtn" onclick="cmd('menu',0)">Menu</button>
  </div>

  <div class="pad-container">
    <div class="ok-btn" id="bOK" aria-label="Select"></div>

    <div class="dpad">
      <div></div>
      <div class="btn" id="bU">▲</div>
      <div></div>
      <div class="btn" id="bL">◄</div>
      <div></div>
      <div class="btn" id="bR">►</div>
      <div></div>
      <div class="btn" id="bD">▼</div>
      <div></div>
    </div>
  </div>

<script>
const pSel = document.getElementById('playerSel');
function sendInput(dx, dy, ok) {
  fetch(`/in?p=${pSel.value}&x=${dx}&y=${dy}&ok=${ok}`).catch(()=>{});
}
function cmd(act, val) {
  fetch(`/cmd?a=${act}&v=${val}`).catch(()=>{});
}
function bindDir(id, dx, dy) {
  const el = document.getElementById(id);
  const down = (e) => { e.preventDefault(); el.classList.add('active'); sendInput(dx, dy, 0); };
  const up = (e) => { e.preventDefault(); el.classList.remove('active'); sendInput(0, 0, 0); };
  el.addEventListener('touchstart', down, {passive: false});
  el.addEventListener('touchend', up, {passive: false});
  el.addEventListener('touchcancel', up, {passive: false});
  el.addEventListener('mousedown', down);
  el.addEventListener('mouseup', up);
  el.addEventListener('mouseleave', up);
}
bindDir('bU', 0, -1);
bindDir('bD', 0, 1);
bindDir('bL', -1, 0);
bindDir('bR', 1, 0);

const bOK = document.getElementById('bOK');
bOK.addEventListener('touchstart', (e)=>{ e.preventDefault(); sendInput(0, 0, 1); }, {passive: false});
bOK.addEventListener('mousedown', (e)=>{ e.preventDefault(); sendInput(0, 0, 1); });
</script>
</body>
</html>
)rawliteral";

// Biến Snake 2 người chơi
constexpr int SN = 15, SMAX = 90;
struct SnakePlayer {
  bool active;
  bool alive;
  int len;
  int dx, dy;
  int x[SMAX], y[SMAX];
  uint16_t color;
};
SnakePlayer snakes[2];
int fx = 8, fy = 7;

void handleWebRoot() {
  server.send_P(200, "text/html", INDEX_HTML);
}

void handleWebInput() {
  int p = constrain(server.arg("p").toInt(), 0, 1);
  int x = constrain(server.arg("x").toInt(), -1, 1);
  int y = constrain(server.arg("y").toInt(), -1, 1);
  int ok = server.arg("ok").toInt();

  webJoyX[p] = x;
  webJoyY[p] = y;
  webLastSeen[p] = millis();
  if (ok == 1) webOkPressed[p] = true;

  if (state == SNAKE && p == 1 && !snakes[1].active) {
    snakes[1].active = true;
    snakes[1].alive = true;
    snakes[1].len = 3;
    snakes[1].dx = -1;
    snakes[1].dy = 0;
    for (int i = 0; i < 3; i++) {
      snakes[1].x[i] = 10 + i;
      snakes[1].y[i] = 10;
    }
  }
  server.send(200, "text/plain", "OK");
}

void handleWebCmd() {
  String a = server.arg("a");
  int v = server.arg("v").toInt();
  if (a == "start") startGame(constrain(v, 0, 3));
  else if (a == "menu") drawMenu();
  server.send(200, "text/plain", "OK");
}

void startWebServer() {
  if (webServerRunning) return;
  server.on("/", HTTP_GET, handleWebRoot);
  server.on("/in", HTTP_GET, handleWebInput);
  server.on("/cmd", HTTP_GET, handleWebCmd);
  server.begin();
  webServerRunning = true;
}

void stopWebServer() {
  if (!webServerRunning) return;
  server.stop();
  webServerRunning = false;
}

// =========================================================================
// BÀN PHÍM ẢO VIRTUAL KEYBOARD + JOYSTICK
// =========================================================================
struct VkbKeyBox {
  int x, y, w, h;
  String label;
  bool isSpecial;
};

static std::vector<VkbKeyBox> vkbRenderedKeys;
static int vkbSelectedIdx = 0;

void vkbDrawCallback(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed) {
  VkbKeyBox box;
  box.x = x; box.y = y; box.w = w; box.h = h;
  box.label = String(label);
  box.isSpecial = isSpecial;
  vkbRenderedKeys.push_back(box);

  bool activeShift = (box.label == "^" && isPressed);
  uint16_t bg = activeShift ? BLUE : (isSpecial ? 0x2945 : 0x10A2);
  uint16_t fg = WHITE;

  tft.fillRoundRect(x, y, w, h, 2, bg);
  tft.drawRoundRect(x, y, w, h, 2, GRAY);

  tft.setTextSize(1);
  tft.setTextColor(fg, bg);

  String disp = box.label;
  if (disp == "Laban Key") disp = "SPACE";
  else if (disp == " spaces") disp = ":";

  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(disp.c_str(), 0, 0, &x1, &y1, &tw, &th);
  int tx = x + max(1, (w - (int)tw) / 2);
  int ty = y + max(1, (h - (int)th) / 2);
  tft.setCursor(tx, ty);
  tft.print(disp);
}

void vkbHighlightKey(int idx, bool active) {
  if (idx < 0 || idx >= (int)vkbRenderedKeys.size()) return;
  const VkbKeyBox& k = vkbRenderedKeys[idx];
  tft.drawRoundRect(k.x, k.y, k.w, k.h, 2, active ? YELLOW : GRAY);
}

String openVirtualKeyboardWithJoystick(const String& wifiName) {
  tft.setRotation(1);
  tft.fillScreen(BLACK);

  String inputStr = "";
  vkbRenderedKeys.clear();
  vkbSelectedIdx = 0;

  vkb.begin(160, 128, 37, vkbDrawCallback);
  vkb.setMode(MODE_LOWERCASE);
  if (!vkbRenderedKeys.empty()) vkbHighlightKey(vkbSelectedIdx, true);

  String safeHeader = "WiFi: " + sanitizeWifiLabel(wifiName);
  if (safeHeader.length() > 25) safeHeader = safeHeader.substring(0, 23) + "..";

  auto redrawHeader = [&]() {
    tft.fillRect(0, 0, 160, 36, BLACK);
    tft.setTextSize(1);
    tft.setTextColor(CYAN, BLACK);
    tft.setCursor(3, 4);
    tft.print(safeHeader);

    tft.drawRect(2, 16, 156, 18, WHITE);
    tft.setTextColor(YELLOW, BLACK);
    tft.setCursor(5, 21);
    String shown = inputStr;
    if (shown.length() > 23) shown = shown.substring(shown.length() - 23);
    tft.print(shown + "_");
  };

  redrawHeader();
  uint32_t lastNavMs = 0;

  while (true) {
    if (webServerRunning) server.handleClient();
    updateInput();
    updateBgm();
    uint32_t now = millis();

    int screenMoveX = joyY;
    int screenMoveY = -joyX;

    if ((screenMoveX != 0 || screenMoveY != 0) && now - lastNavMs > 160 && !vkbRenderedKeys.empty()) {
      lastNavMs = now;
      int oldIdx = vkbSelectedIdx;

      if (screenMoveX != 0) {
        vkbSelectedIdx = (vkbSelectedIdx + (screenMoveX > 0 ? 1 : (int)vkbRenderedKeys.size() - 1)) % (int)vkbRenderedKeys.size();
      } else if (screenMoveY != 0) {
        const VkbKeyBox& cur = vkbRenderedKeys[vkbSelectedIdx];
        int curCx = cur.x + cur.w / 2;
        int curCy = cur.y + cur.h / 2;
        int bestIdx = vkbSelectedIdx;
        int bestDist = 999999;

        for (int i = 0; i < (int)vkbRenderedKeys.size(); i++) {
          int ny = vkbRenderedKeys[i].y + vkbRenderedKeys[i].h / 2;
          if ((screenMoveY > 0 && ny > curCy + 8) || (screenMoveY < 0 && ny < curCy - 8)) {
            int nx = vkbRenderedKeys[i].x + vkbRenderedKeys[i].w / 2;
            int scoreDist = abs(ny - curCy) * 10 + abs(nx - curCx);
            if (scoreDist < bestDist) {
              bestDist = scoreDist;
              bestIdx = i;
            }
          }
        }
        vkbSelectedIdx = bestIdx;
      }

      vkbHighlightKey(oldIdx, false);
      vkbHighlightKey(vkbSelectedIdx, true);
    }

    if (shortPress && !vkbRenderedKeys.empty()) {
      playClickSfx();
      const VkbKeyBox& sel = vkbRenderedKeys[vkbSelectedIdx];
      String lbl = sel.label;
      KeyMode curMode = vkb.getMode();

      if (lbl == "^") {
        vkbRenderedKeys.clear();
        vkb.setMode(curMode == MODE_LOWERCASE ? MODE_UPPERCASE : MODE_LOWERCASE);
        if (vkbSelectedIdx >= (int)vkbRenderedKeys.size()) vkbSelectedIdx = 0;
        vkbHighlightKey(vkbSelectedIdx, true);
      } else if (lbl == "123") {
        vkbRenderedKeys.clear();
        vkb.setMode(MODE_NUMBERS);
        if (vkbSelectedIdx >= (int)vkbRenderedKeys.size()) vkbSelectedIdx = 0;
        vkbHighlightKey(vkbSelectedIdx, true);
      } else if (lbl == "#+=") {
        vkbRenderedKeys.clear();
        vkb.setMode(MODE_SYMBOLS);
        if (vkbSelectedIdx >= (int)vkbRenderedKeys.size()) vkbSelectedIdx = 0;
        vkbHighlightKey(vkbSelectedIdx, true);
      } else if (lbl == "ABC") {
        vkbRenderedKeys.clear();
        vkb.setMode(MODE_LOWERCASE);
        if (vkbSelectedIdx >= (int)vkbRenderedKeys.size()) vkbSelectedIdx = 0;
        vkbHighlightKey(vkbSelectedIdx, true);
      } else if (lbl == "Nhap") {
        break;
      } else if (lbl == "<X") {
        if (inputStr.length() > 0) inputStr.remove(inputStr.length() - 1);
        redrawHeader();
      } else if (lbl == "Laban Key") {
        inputStr += " ";
        redrawHeader();
      } else if (lbl == " spaces") {
        inputStr += ":";
        redrawHeader();
      } else {
        inputStr += lbl;
        redrawHeader();
      }
    }

    if (longPress) {
      if (inputStr.length() > 0) {
        inputStr.remove(inputStr.length() - 1);
        redrawHeader();
        playClickSfx();
      } else {
        break;
      }
    }
    delay(10);
  }

  tft.setRotation(0);
  return inputStr;
}

// =========================================================================
// LƯU TRỮ PREFERENCES & QUẢN LÝ WI-FI / WEB SERVER
// =========================================================================
void loadSettings() {
  prefs.begin("minigame", true);
  best[0] = prefs.getInt("snake", 0);
  best[1] = prefs.getInt("tetris", 0);
  best[2] = prefs.getInt("pacman", 0);
  best[3] = prefs.getInt("breakout", 0);
  level[0] = constrain(prefs.getInt("snakeLv", 1), 1, MAX_LEVEL);
  level[1] = constrain(prefs.getInt("tetrisLv", 1), 1, MAX_LEVEL);
  level[2] = constrain(prefs.getInt("pacmanLv", 1), 1, MAX_LEVEL);
  level[3] = constrain(prefs.getInt("breakoutLv", 1), 1, MAX_LEVEL);

  bgmEnabled = prefs.getBool("bgm", true);
  soundVolume = constrain(prefs.getInt("vol", 5), 0, 10);
  savedSsid = prefs.getString("ssid", "");
  savedPass = prefs.getString("pass", "");
  prefs.end();
}

void saveAudioSettings() {
  prefs.begin("minigame", false);
  prefs.putBool("bgm", bgmEnabled);
  prefs.putInt("vol", soundVolume);
  prefs.end();
}

void saveWifiCredentials(const String& s, const String& p) {
  savedSsid = s;
  savedPass = p;
  prefs.begin("minigame", false);
  prefs.putString("ssid", savedSsid);
  prefs.putString("pass", savedPass);
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

void turnOffAllWifi() {
  stopWebServer();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
  currentWifiSubMode = WIFI_OFF_MODE;
}

void enableWifiStaMode() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  currentWifiSubMode = WIFI_STA_MODE;
  if (savedSsid.length() > 0) {
    WiFi.begin(savedSsid.c_str(), savedPass.c_str());
  }
  startWebServer();
}

void enableWifiApMode() {
  WiFi.disconnect(true);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(apSsid, apPass, WIFI_CHANNEL);
  currentWifiSubMode = WIFI_AP_MODE;
  startWebServer();
}

// =========================================================================
// GIAO THỨC ESP-NOW POWERPOINT
// =========================================================================
void onDataRecv(const esp_now_recv_info_t* info, const uint8_t* incomingData, int len) {
  if (len != sizeof(DataPacket)) return;

  DataPacket pkt;
  memcpy(&pkt, incomingData, sizeof(DataPacket));
  if (pkt.magic != 0x5A77) return;

  lastPacketMs = millis();

  if (pkt.type == 1) {
    uint16_t row = pkt.param1;
    uint16_t byteOffset = pkt.param2;
    if (row < IMG_H && (byteOffset + pkt.len) <= (IMG_W * 2)) {
      portENTER_CRITICAL_ISR(&mux);
      uint8_t* dst = (uint8_t*)&currImgBuffer[row * IMG_W] + byteOffset;
      memcpy(dst, pkt.data, pkt.len);
      if (row == IMG_H - 1 && byteOffset + pkt.len == IMG_W * 2) {
        currImgDirty = true;
      }
      portEXIT_CRITICAL_ISR(&mux);
    }
  } else if (pkt.type == 4) {
    uint16_t row = pkt.param1;
    uint16_t byteOffset = pkt.param2;
    if (row < IMG_H && (byteOffset + pkt.len) <= (IMG_W * 2)) {
      portENTER_CRITICAL_ISR(&mux);
      uint8_t* dst = (uint8_t*)&nextImgBuffer[row * IMG_W] + byteOffset;
      memcpy(dst, pkt.data, pkt.len);
      if (row == IMG_H - 1 && byteOffset + pkt.len == IMG_W * 2) {
        nextImgDirty = true;
      }
      portEXIT_CRITICAL_ISR(&mux);
    }
  } else if (pkt.type == 2) {
    uint16_t offset = pkt.param1;
    uint16_t totalLen = pkt.param2;
    if (totalLen < sizeof(rxTextBuf) - 1 && (offset + pkt.len) <= totalLen) {
      if (offset == 0) {
        rxExpectedLen = totalLen;
        rxReceivedBytes = 0;
        memset(rxTextBuf, 0, sizeof(rxTextBuf));
      }
      memcpy(rxTextBuf + offset, pkt.data, pkt.len);
      rxReceivedBytes += pkt.len;

      if (rxReceivedBytes >= totalLen || (offset + pkt.len) == totalLen) {
        rxTextBuf[totalLen] = '\0';
        portENTER_CRITICAL_ISR(&mux);
        memcpy(slideTextBuf, rxTextBuf, totalLen + 1);
        textUpdated = true;
        portEXIT_CRITICAL_ISR(&mux);
      }
    }
  }
}

void sendPptCommand(uint8_t cmdCode) {
  if (!espNowInitialized) return;
  CmdPacket pkt;
  pkt.magic = 0x5A78;
  pkt.cmd = cmdCode;
  pkt.checksum = cmdCode ^ 0xAA;
  esp_now_send(c3PeerMac, (uint8_t*)&pkt, sizeof(CmdPacket));
}

// =========================================================================
// GIAO DIỆN ĐỒ HỌA CHUNG & MENU CHÍNH
// =========================================================================
void centered(const char* s, int y, uint16_t c, uint8_t size = 1) {
  tft.setTextSize(size);
  tft.setTextColor(c, BLACK);
  int16_t x1, y1;
  uint16_t tw, th;
  tft.getTextBounds(s, 0, y, &x1, &y1, &tw, &th);
  tft.setCursor((W - tw) / 2, y);
  tft.print(s);
}

void drawMenuRow(int i, bool selected) {
  const char* names[] = {"SNAKE", "TETRIS", "PAC-MAN", "BREAKOUT", "POWERPOINT", "SETTING"};
  const uint16_t rowColors[] = {GREEN, CYAN, YELLOW, ORANGE, MAGENTA, WHITE};
  int y = 26 + i * 21;
  uint16_t c = rowColors[i];

  if (selected) {
    tft.fillRoundRect(4, y, 120, 19, 4, c);
    tft.setTextColor(BLACK, c);
  } else {
    tft.fillRoundRect(4, y, 120, 19, 4, BLACK);
    tft.drawRoundRect(4, y, 120, 19, 4, c);
    tft.setTextColor(c, BLACK);
  }

  tft.setTextSize(1);
  tft.setCursor(9, y + 6);
  tft.print(names[i]);

  if (i < 4) {
    tft.setCursor(100, y + 6);
    tft.print("L");
    tft.print(level[i]);
  }
}

void drawMenu() {
  stopBgm();
  state = MENU;
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered("MINI GAME V3", 7, CYAN, 1);
  tft.drawFastHLine(SAFE_X, 20, SAFE_W, GRAY);
  for (int i = 0; i < 6; i++) drawMenuRow(i, i == menuIndex);
}

// =========================================================================
// MỤC POWERPOINT REMOTE (ĐỒNG BỘ TRẠNG THÁI VỚI LED GPIO 8 CỦA C3)
// =========================================================================
void drawStatusDot() {
  uint16_t dotColor = ST77XX_RED;
  if (millis() - lastPacketMs < 3500) {
    if (contentMode == CONTENT_CURR_SLIDE) dotColor = ST77XX_GREEN;
    else if (contentMode == CONTENT_TEXT) dotColor = ST77XX_CYAN;
    else if (contentMode == CONTENT_NEXT_SLIDE) dotColor = ST77XX_YELLOW;
  }

  if (orientMode == ORIENT_LANDSCAPE) tft.fillRect(156, 1, 3, 3, dotColor);
  else tft.fillRect(124, 1, 3, 3, dotColor);
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
  tft.fillRect(x0, y0, boxW, boxH, ST77XX_BLACK);
  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setForegroundColor(ST77XX_WHITE);
  u8g2.setFont(u8g2_font_unifont_t_vietnamese2);

  const int maxWidth = boxW - 4;
  const int lineHeight = 14;
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
      if (u8g2.getUTF8Width(testLine.c_str()) <= maxWidth) {
        currentLine = testLine;
      } else {
        if (currentLine.length() > 0) lines.push_back(currentLine);
        currentLine = word;
      }
    }
    if (currentLine.length() > 0) lines.push_back(currentLine);
  }

  totalWrappedLines = (int)lines.size();
  int maxScroll = max(0, totalWrappedLines - maxVisibleLines);
  if (textScrollLine > maxScroll) textScrollLine = maxScroll;
  if (textScrollLine < 0) textScrollLine = 0;

  int yCursor = y0 + 13;
  for (int i = textScrollLine; i < min(totalWrappedLines, textScrollLine + maxVisibleLines); i++) {
    u8g2.setCursor(x0 + 2, yCursor);
    u8g2.print(lines[i]);
    yCursor += lineHeight;
  }
}

void renderPptFullScreen() {
  tft.fillScreen(ST77XX_BLACK);
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
  drawStatusDot();
}

void pptSinglePress() {
  if (orientMode == ORIENT_LANDSCAPE) {
    contentMode = (contentMode == CONTENT_CURR_SLIDE) ? CONTENT_TEXT : CONTENT_CURR_SLIDE;
    textScrollLine = 0;
  } else {
    contentMode = CONTENT_TEXT;
    textScrollLine = 0;
  }
  renderPptFullScreen();
}

void pptDoublePress() {
  if (orientMode == ORIENT_LANDSCAPE) {
    contentMode = (contentMode == CONTENT_NEXT_SLIDE) ? CONTENT_CURR_SLIDE : CONTENT_NEXT_SLIDE;
  } else {
    contentMode = CONTENT_NEXT_SLIDE;
  }
  renderPptFullScreen();
}

void pptLongPress() {
  if (orientMode == ORIENT_LANDSCAPE) {
    orientMode = ORIENT_PORTRAIT;
    tft.setRotation(0);
    contentMode = CONTENT_TEXT;
    textScrollLine = 0;
  } else {
    orientMode = ORIENT_LANDSCAPE;
    tft.setRotation(1);
    contentMode = CONTENT_CURR_SLIDE;
  }
  renderPptFullScreen();
}

void startPowerPoint() {
  state = POWERPOINT;
  stopBgm();
  orientMode = ORIENT_LANDSCAPE;
  contentMode = CONTENT_CURR_SLIDE;
  tft.setRotation(1);
  tft.fillScreen(ST77XX_BLACK);

  memset(currImgBuffer, 0, sizeof(currImgBuffer));
  memset(nextImgBuffer, 0, sizeof(nextImgBuffer));
  strcpy(slideTextBuf, "Đang kết nối máy tính...\n(Nhấn giữ >2s để thoát)");
  currImgDirty = false;
  nextImgDirty = false;
  textUpdated = false;
  lastPacketMs = 0;

  stopWebServer();
  WiFi.softAPdisconnect(true);
  WiFi.disconnect(true);
  delay(30);
  WiFi.mode(WIFI_STA);
  delay(30);
  esp_wifi_set_ps(WIFI_PS_NONE);
  esp_wifi_set_promiscuous(true);
  esp_wifi_set_channel(WIFI_CHANNEL, WIFI_SECOND_CHAN_NONE);
  esp_wifi_set_promiscuous(false);

  if (espNowInitialized) {
    esp_now_deinit();
    espNowInitialized = false;
  }

  if (esp_now_init() == ESP_OK) {
    espNowInitialized = true;
    esp_now_register_recv_cb(onDataRecv);

    esp_now_peer_info_t peerInfo = {};
    memcpy(peerInfo.peer_addr, c3PeerMac, 6);
    peerInfo.channel = WIFI_CHANNEL;
    peerInfo.ifidx = WIFI_IF_STA;
    peerInfo.encrypt = false;
    esp_now_add_peer(&peerInfo);

    // Gửi ngay gói PONG để LED GPIO 8 của C3 nhận diện S3 tức thì
    sendPptCommand(3);
  }

  renderPptFullScreen();
}

void stopPowerPoint() {
  if (espNowInitialized) {
    esp_now_deinit();
    espNowInitialized = false;
  }
  if (currentWifiSubMode == WIFI_STA_MODE) enableWifiStaMode();
  else if (currentWifiSubMode == WIFI_AP_MODE) enableWifiApMode();
  else turnOffAllWifi();
  drawMenu();
}

void runPowerPoint() {
  static unsigned long lastMoveMs = 0;
  static bool btnPressed = false;
  static bool longPressHandled = false;
  static bool exitHandled = false;
  static unsigned long btnDownMs = 0;
  static uint8_t clickCount = 0;
  static unsigned long firstReleaseMs = 0;
  static unsigned long lastDotMs = 0;
  static unsigned long lastPongMs = 0;

  unsigned long now = millis();

  // Gửi nhịp tim PONG mỗi 1.5s để duy trì trạng thái LED GPIO 8 sáng đứng trên C3
  if (now - lastPongMs >= 1500) {
    lastPongMs = now;
    sendPptCommand(3);
  }

  bool rawBtn = hwControl.readRawSelectButton();

  if (rawBtn && !btnPressed) {
    btnPressed = true;
    longPressHandled = false;
    exitHandled = false;
    btnDownMs = now;
  } else if (rawBtn && btnPressed) {
    unsigned long held = now - btnDownMs;
    if (held >= 2200 && !exitHandled) {
      exitHandled = true;
      clickCount = 0;
      stopPowerPoint();
      playClickSfx();
      return;
    } else if (held >= 550 && !longPressHandled) {
      longPressHandled = true;
      clickCount = 0;
      pptLongPress();
    }
  } else if (!rawBtn && btnPressed) {
    btnPressed = false;
    if (!longPressHandled && !exitHandled) {
      unsigned long holdMs = now - btnDownMs;
      if (holdMs > 20 && holdMs < 550) {
        clickCount++;
        if (clickCount == 1) firstReleaseMs = now;
        else if (clickCount == 2) { clickCount = 0; pptDoublePress(); }
      }
    }
  }

  if (clickCount == 1 && (now - firstReleaseMs > 320)) {
    clickCount = 0;
    pptSinglePress();
  }

  if (now - lastMoveMs >= 240) {
    int rawX = hwControl.readRawX();
    int rawY = hwControl.readRawY();
    int navAxis = (orientMode == ORIENT_LANDSCAPE) ? rawY : rawX;
    int scrollAxis = (orientMode == ORIENT_LANDSCAPE) ? rawX : (4095 - rawY);

    if (navAxis < 700) {
      lastMoveMs = now;
      sendPptCommand(2);
    } else if (navAxis > 3400) {
      lastMoveMs = now;
      sendPptCommand(1);
    } else if (contentMode == CONTENT_TEXT) {
      int visibleLines = (orientMode == ORIENT_LANDSCAPE) ? 8 : 5;
      if (scrollAxis < 700 && textScrollLine > 0) {
        lastMoveMs = now;
        textScrollLine--;
        renderPptFullScreen();
      } else if (scrollAxis > 3400 && textScrollLine < max(0, totalWrappedLines - visibleLines)) {
        lastMoveMs = now;
        textScrollLine++;
        renderPptFullScreen();
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
    textUpdated = false;
    textScrollLine = 0;
    if (contentMode == CONTENT_TEXT) renderPptFullScreen();
  }

  if (now - lastDotMs > 200) {
    lastDotMs = now;
    drawStatusDot();
  }
}

// =========================================================================
// MỤC SETTING (WIFI / SOUND / RESET / BACK TO MENU)
// =========================================================================
void resetEverything() {
  prefs.begin("minigame", false);
  prefs.clear();
  prefs.end();

  for (int i = 0; i < 4; i++) { best[i] = 0; level[i] = 1; }
  bgmEnabled = true;
  soundVolume = 5;
  savedSsid = "";
  savedPass = "";
  score = 0;
  resetPressCount = 0;
  settingIndex = 0;
  gameWon = false;
  playWinSfx();
  drawSettingMenu();
}

void drawSettingRow(int i, bool selected) {
  const char* labels[] = {"WIFI", "SOUND", "RESET x5", "BACK TO MENU"};
  const uint16_t colors[] = {CYAN, GREEN, RED, WHITE};
  int y = 30 + i * 28;
  uint16_t c = colors[i];

  if (selected) {
    tft.fillRoundRect(4, y, 120, 23, 4, c);
    tft.setTextColor(BLACK, c);
  } else {
    tft.fillRoundRect(4, y, 120, 23, 4, BLACK);
    tft.drawRoundRect(4, y, 120, 23, 4, c);
    tft.setTextColor(c, BLACK);
  }

  tft.setTextSize(1);
  tft.setCursor(9, y + 8);
  tft.print(labels[i]);

  if (i == 2) {
    tft.setCursor(98, y + 8);
    tft.print(resetPressCount);
    tft.print("/5");
  }
}

void drawSettingMenu() {
  state = SETTING;
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered("SETTING", 7, WHITE, 1);
  tft.drawFastHLine(SAFE_X, 20, SAFE_W, GRAY);
  for (int i = 0; i < 4; i++) drawSettingRow(i, i == settingIndex);
}

void runSettingMenu() {
  if (joyY && millis() - lastInput > 170) {
    lastInput = millis();
    int old = settingIndex;
    settingIndex = (settingIndex + (joyY > 0 ? 1 : 3)) % 4;
    drawSettingRow(old, false);
    drawSettingRow(settingIndex, true);
  }

  if (shortPress) {
    playClickSfx();
    if (settingIndex == 0) { wifiMenuIndex = 0; drawWifiMenu(); }
    else if (settingIndex == 1) { soundMenuIndex = 0; drawSoundMenu(); }
    else if (settingIndex == 2) {
      uint32_t now = millis();
      if (now - resetPressAt > 3000) resetPressCount = 0;
      resetPressAt = now;
      resetPressCount++;
      drawSettingRow(2, true);
      if (resetPressCount >= 5) resetEverything();
    } else if (settingIndex == 3) {
      resetPressCount = 0;
      drawMenu();
    }
  }
  if (longPress) {
    resetPressCount = 0;
    drawMenu();
  }
}

// =========================================================================
// MỤC SETTING -> WIFI
// =========================================================================
void drawWifiStatusBox() {
  tft.fillRect(4, 92, 120, 64, BLACK);
  tft.drawFastHLine(4, 91, 120, GRAY);

  u8g2.setFontMode(1);
  u8g2.setFontDirection(0);
  u8g2.setFont(u8g2_font_5x8_tf);

  auto fitLine = [](String s) -> String {
    if (s.length() > 22) s = s.substring(0, 20) + "..";
    return s;
  };

  String stText = "STATUS: ";
  String nameText = "NAME: ";
  String passText = "PASS: ";
  String ipText = "IP: ";

  if (currentWifiSubMode == WIFI_STA_MODE) {
    if (WiFi.status() == WL_CONNECTED) {
      stText += "OK (2P WEB ON)";
      nameText += sanitizeWifiLabel(WiFi.SSID());
      passText += (savedPass.length() > 0 ? savedPass : "(None)");
      ipText += WiFi.localIP().toString();
    } else {
      stText += "CONNECTING...";
      nameText += (savedSsid.length() > 0 ? sanitizeWifiLabel(savedSsid) : "---");
      passText += (savedPass.length() > 0 ? savedPass : "---");
      ipText += "0.0.0.0";
    }
  } else if (currentWifiSubMode == WIFI_AP_MODE) {
    stText += "AP (2P WEB ON)";
    nameText += String(apSsid);
    passText += String(apPass);
    ipText += WiFi.softAPIP().toString();
  } else {
    stText += "OFF";
    nameText += "---";
    passText += "---";
    ipText += "---";
  }

  u8g2.setForegroundColor(GREEN);
  u8g2.drawUTF8(6, 103, fitLine(stText).c_str());

  u8g2.setForegroundColor(CYAN);
  u8g2.drawUTF8(6, 117, fitLine(nameText).c_str());

  u8g2.setForegroundColor(YELLOW);
  u8g2.drawUTF8(6, 131, fitLine(passText).c_str());

  u8g2.setForegroundColor(WHITE);
  u8g2.drawUTF8(6, 145, fitLine(ipText).c_str());
}

void drawWifiRow(int i, bool selected) {
  const char* opts[] = {"WIFI (SCAN & PASS)", "ACCESS POINT", "BACK TO SETTING"};
  const uint16_t cols[] = {CYAN, YELLOW, WHITE};
  int y = 22 + i * 22;
  uint16_t c = cols[i];

  if (selected) {
    tft.fillRoundRect(4, y, 120, 20, 4, c);
    tft.setTextColor(BLACK, c);
  } else {
    tft.fillRoundRect(4, y, 120, 20, 4, BLACK);
    tft.drawRoundRect(4, y, 120, 20, 4, c);
    tft.setTextColor(c, BLACK);
  }
  tft.setTextSize(1);
  tft.setCursor(8, y + 6);
  tft.print(opts[i]);
}

void drawWifiMenu() {
  state = SET_WIFI;
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered("WIFI SETTING", 6, CYAN, 1);
  tft.drawFastHLine(SAFE_X, 18, SAFE_W, GRAY);
  for (int i = 0; i < 3; i++) drawWifiRow(i, i == wifiMenuIndex);
  drawWifiStatusBox();
}

void drawScanListItem(int itemIdx, int visibleRow, bool sel) {
  int totalItems = (int)scannedSsids.size() + 1;
  int y = 22 + visibleRow * 21;
  bool isBackRow = (itemIdx == totalItems - 1);

  uint16_t borderCol = isBackRow ? YELLOW : GRAY;
  uint16_t fillCol = sel ? (isBackRow ? YELLOW : CYAN) : BLACK;

  tft.fillRoundRect(4, y, 120, 19, 3, fillCol);
  if (!sel) tft.drawRoundRect(4, y, 120, 19, 3, borderCol);

  tft.setTextSize(1);
  if (isBackRow) {
    tft.setTextColor(sel ? BLACK : YELLOW, fillCol);
    tft.setCursor(8, y + 6);
    tft.print("< BACK TO WIFI SET");
  } else {
    tft.setTextColor(sel ? BLACK : WHITE, fillCol);
    tft.setCursor(8, y + 6);
    String label = sanitizeWifiLabel(scannedSsids[itemIdx]);
    if (label.length() > 18) label = label.substring(0, 16) + "..";
    tft.print(label);
  }
}

void renderWifiScanPage() {
  int totalItems = (int)scannedSsids.size() + 1;
  tft.fillRect(3, 21, 122, 134, BLACK);

  int endIdx = min(totalItems, wifiScrollOffset + WIFI_VISIBLE_ROWS);
  for (int i = wifiScrollOffset; i < endIdx; i++) {
    drawScanListItem(i, i - wifiScrollOffset, i == scanSelectedIdx);
  }
}

void startWifiScanAndConnect() {
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  currentWifiSubMode = WIFI_STA_MODE;

  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered("DANG QUET WIFI...", 70, CYAN, 1);

  int n = WiFi.scanNetworks();
  scannedSsids.clear();
  for (int i = 0; i < n; i++) {
    String s = WiFi.SSID(i);
    if (s.length() > 0) {
      bool exists = false;
      for (const auto& existing : scannedSsids) {
        if (existing == s) { exists = true; break; }
      }
      if (!exists) scannedSsids.push_back(s);
    }
  }
  WiFi.scanDelete();

  scanSelectedIdx = 0;
  wifiScrollOffset = 0;

  state = SET_WIFI_SCAN;
  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered("CHON MANG WIFI", 6, CYAN, 1);
  tft.drawFastHLine(SAFE_X, 18, SAFE_W, GRAY);

  renderWifiScanPage();
}

void runWifiScanSelect() {
  int totalItems = (int)scannedSsids.size() + 1;
  if (joyY && millis() - lastInput > 170) {
    lastInput = millis();
    int oldIdx = scanSelectedIdx;
    int oldOffset = wifiScrollOffset;

    scanSelectedIdx = (scanSelectedIdx + (joyY > 0 ? 1 : totalItems - 1)) % totalItems;

    if (scanSelectedIdx < wifiScrollOffset) {
      wifiScrollOffset = scanSelectedIdx;
    } else if (scanSelectedIdx >= wifiScrollOffset + WIFI_VISIBLE_ROWS) {
      wifiScrollOffset = scanSelectedIdx - WIFI_VISIBLE_ROWS + 1;
    }

    if (wifiScrollOffset != oldOffset) {
      renderWifiScanPage();
    } else {
      drawScanListItem(oldIdx, oldIdx - wifiScrollOffset, false);
      drawScanListItem(scanSelectedIdx, scanSelectedIdx - wifiScrollOffset, true);
    }
  }

  if (shortPress) {
    playClickSfx();
    if (scanSelectedIdx == totalItems - 1) {
      drawWifiMenu();
      return;
    }

    String chosenSsid = scannedSsids[scanSelectedIdx];
    String enteredPass = openVirtualKeyboardWithJoystick(chosenSsid);

    saveWifiCredentials(chosenSsid, enteredPass);
    enableWifiStaMode();

    tft.fillScreen(BLACK);
    centered("DANG KET NOI...", 65, YELLOW, 1);
    uint32_t t0 = millis();
    while (WiFi.status() != WL_CONNECTED && millis() - t0 < 5000) {
      delay(200);
    }
    drawWifiMenu();
  }

  if (longPress) {
    playClickSfx();
    drawWifiMenu();
  }
}

void runWifiMenu() {
  static uint32_t lastStatusRefresh = 0;
  if (millis() - lastStatusRefresh > 1000) {
    lastStatusRefresh = millis();
    drawWifiStatusBox();
  }

  if (joyY && millis() - lastInput > 170) {
    lastInput = millis();
    int old = wifiMenuIndex;
    wifiMenuIndex = (wifiMenuIndex + (joyY > 0 ? 1 : 2)) % 3;
    drawWifiRow(old, false);
    drawWifiRow(wifiMenuIndex, true);
  }

  if (shortPress) {
    playClickSfx();
    if (wifiMenuIndex == 0) {
      startWifiScanAndConnect();
    } else if (wifiMenuIndex == 1) {
      enableWifiApMode();
      drawWifiStatusBox();
    } else if (wifiMenuIndex == 2) {
      drawSettingMenu();
    }
  }

  if (longPress) {
    playClickSfx();
    drawSettingMenu();
  }
}

// =========================================================================
// MỤC SETTING -> SOUND
// =========================================================================
void drawSoundRow(int i, bool selected) {
  int y = 30 + i * 28;
  uint16_t c = (i == 0) ? GREEN : (i == 1 ? CYAN : WHITE);

  if (selected) {
    tft.fillRoundRect(4, y, 120, 23, 4, c);
    tft.setTextColor(BLACK, c);
  } else {
    tft.fillRoundRect(4, y, 120, 23, 4, BLACK);
    tft.drawRoundRect(4, y, 120, 23, 4, c);
    tft.setTextColor(c, BLACK);
  }

  tft.setTextSize(1);
  tft.setCursor(9, y + 8);
  if (i == 0) {
    tft.print("NHAC NEN: ");
    tft.print(bgmEnabled ? "BAT" : "TAT");
  } else if (i == 1) {
    tft.print("AM LUONG: ");
    tft.print(soundVolume * 10);
    tft.print("%");
  } else {
    tft.print("BACK TO SETTING");
  }
}

void drawSoundVolumeBar() {
  tft.fillRect(6, 118, 116, 16, BLACK);
  tft.drawRect(6, 118, 116, 16, GRAY);
  for (int lv = 1; lv <= 10; lv++) {
    uint16_t barCol = (lv <= soundVolume) ? GREEN : 0x2104;
    tft.fillRect(9 + (lv - 1) * 11, 121, 9, 10, barCol);
  }
}

void drawSoundMenu() {
  state = SET_SOUND;
  tft.setRotation(0);
  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered("SOUND SETTING", 7, GREEN, 1);
  tft.drawFastHLine(SAFE_X, 20, SAFE_W, GRAY);
  for (int i = 0; i < 3; i++) drawSoundRow(i, i == soundMenuIndex);
  drawSoundVolumeBar();
  centered("TRAI/PHAI: CHINH MUC", 142, GRAY, 1);
}

void runSoundMenu() {
  if (joyY && millis() - lastInput > 170) {
    lastInput = millis();
    int old = soundMenuIndex;
    soundMenuIndex = (soundMenuIndex + (joyY > 0 ? 1 : 2)) % 3;
    drawSoundRow(old, false);
    drawSoundRow(soundMenuIndex, true);
  }

  if (soundMenuIndex == 1 && joyX != 0 && millis() - lastInput > 140) {
    lastInput = millis();
    soundVolume = constrain(soundVolume + joyX, 0, 10);
    saveAudioSettings();
    drawSoundRow(1, true);
    drawSoundVolumeBar();
  }

  if (shortPress) {
    if (soundMenuIndex == 0) {
      bgmEnabled = !bgmEnabled;
      saveAudioSettings();
      drawSoundRow(0, true);
      playClickSfx();
    } else if (soundMenuIndex == 1) {
      soundVolume = (soundVolume + 1) % 11;
      saveAudioSettings();
      drawSoundRow(1, true);
      drawSoundVolumeBar();
      playClickSfx();
    } else if (soundMenuIndex == 2) {
      playClickSfx();
      drawSettingMenu();
    }
  }

  if (longPress) {
    playClickSfx();
    drawSettingMenu();
  }
}

// =========================================================================
// 4 TỰA GAME (SNAKE 2P, TETRIS, PAC-MAN, BREAKOUT)
// =========================================================================
float tocDoGame(int lv) {
  return 0.5f + (constrain(lv, 1, MAX_LEVEL) - 1) * 0.25f;
}

uint16_t levelColor(int lv) {
  const uint16_t palette[10] = {GREEN, CYAN, YELLOW, ORANGE, MAGENTA, BLUE, RED, PINK, WHITE, 0x07E0};
  return palette[(constrain(lv, 1, MAX_LEVEL) - 1) % 10];
}

int gameSlot(State s) {
  switch (s) {
    case SNAKE: return 0;
    case TETRIS: return 1;
    case PACMAN: return 2;
    default: return 3;
  }
}

const char* keyFor(State s) {
  switch (s) {
    case SNAKE: return "snake";
    case TETRIS: return "tetris";
    case PACMAN: return "pacman";
    default: return "breakout";
  }
}

void drawLevelDecor(int lv) {
  uint16_t c = levelColor(lv);
  tft.drawRect(2, 2, 124, 156, c);
}

void header(const char* s, uint16_t c) {
  tft.fillRect(SAFE_X, SAFE_Y, SAFE_W, 17, BLACK);
  tft.drawFastHLine(SAFE_X, 20, SAFE_W, levelColor(level[gameSlot(state)]));
  tft.setTextSize(1);
  tft.setTextColor(c, BLACK);
  tft.setCursor(6, 8);
  tft.print(s);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(83, 8);
  tft.print("S:");
  tft.print(score);
}

void scoreDraw() {
  tft.fillRect(80, 5, 43, 12, BLACK);
  tft.setTextSize(1);
  tft.setTextColor(YELLOW, BLACK);
  tft.setCursor(83, 8);
  tft.print("S:");
  tft.print(score);
}

void drawResultChoice() {
  const char* opts[] = {
    gameWon ? "NEXT LEVEL >" : "PLAY AGAIN >", "BACK TO MENU"
  };
  for (int i = 0; i < 2; i++) {
    int y = 112 + i * 22;
    uint16_t edge = (i == resultChoice) ? GREEN : GRAY;
    uint16_t fill = (i == resultChoice) ? GREEN : BLACK;
    tft.fillRoundRect(3, y, 122, 19, 4, fill);
    tft.drawRoundRect(3, y, 122, 19, 4, edge);
    tft.setTextSize(1);
    tft.setTextColor((i == resultChoice) ? BLACK : WHITE, fill);
    int16_t x1, y1;
    uint16_t tw, th;
    tft.getTextBounds(opts[i], 0, y, &x1, &y1, &tw, &th);
    tft.setCursor((W - tw) / 2, y + 6);
    tft.print(opts[i]);
  }
}

void saveResult() {
  stopBgm();
  int slot = gameSlot(lastGame);
  newRecord = score > best[slot];
  if (newRecord) {
    best[slot] = score;
    prefs.begin("minigame", false);
    prefs.putInt(keyFor(lastGame), score);
    prefs.end();
  }
  state = RESULT;
  resultChoice = 0;
  tft.fillScreen(BLACK);
  tft.drawRect(SAFE_X, SAFE_Y, SAFE_W, SAFE_H, GRAY);
  centered(gameWon ? "LEVEL CLEARED!" : "GAME OVER", 12, WHITE, 1);
  centered(gameNames[slot], 25, gameColors[slot], 2);
  char buf[24];
  snprintf(buf, sizeof(buf), "LEVEL: %d / %d", level[slot], MAX_LEVEL);
  centered(buf, 46, CYAN, 1);
  centered(gameWon ? "WELL PLAYED!" : "TRY AGAIN!", 64, gameWon ? GREEN : GRAY, 1);
  centered("UP/DOWN + PRESS", 91, GRAY, 1);
  drawResultChoice();
}

void endGame() {
  lastGame = state;
  if (gameWon) playWinSfx();
  else playGameOverSfx();
  saveResult();
}

void gameOver() { gameWon = false; endGame(); }
void winGame() { gameWon = true; endGame(); }

// ---------- 1. SNAKE (2 NGƯỜI CHƠI) ----------
constexpr int SC = 8, SX = 4, SY = 28;
uint32_t snakeTick = 0;
int snakeDelay = 155;

void snakeCell(int x, int y, uint16_t c) {
  tft.fillRect(SX + x * SC, SY + y * SC, SC - 1, SC - 1, c);
}

bool snakeAnyHas(int x, int y) {
  for (int p = 0; p < 2; p++) {
    if (!snakes[p].active || !snakes[p].alive) continue;
    for (int i = 0; i < snakes[p].len; i++) {
      if (snakes[p].x[i] == x && snakes[p].y[i] == y) return true;
    }
  }
  return false;
}

void foodSpawn() {
  for (int n = 0; n < 250; n++) {
    int x = random(SN), y = random(SN);
    if (!snakeAnyHas(x, y)) {
      fx = x; fy = y;
      snakeCell(fx, fy, RED);
      return;
    }
  }
}

void startSnake() {
  state = SNAKE;
  bgmNoteIdx = 0;
  score = 0;
  snakeDelay = (int)(165.0f / tocDoGame(level[0]));

  const uint16_t pCols[2] = {GREEN, CYAN};
  for (int p = 0; p < 2; p++) {
    snakes[p].color = pCols[p];
    bool isOnline = (p == 0) || (webLastSeen[1] > 0 && millis() - webLastSeen[1] < 15000);
    snakes[p].active = isOnline;
    snakes[p].alive = isOnline;
    snakes[p].len = 3;
    snakes[p].dx = (p == 0) ? 1 : -1;
    snakes[p].dy = 0;
    int sy0 = (p == 0) ? 4 : 10;
    int sx0 = (p == 0) ? 4 : 10;
    for (int i = 0; i < 3; i++) {
      snakes[p].x[i] = sx0 - i * snakes[p].dx;
      snakes[p].y[i] = sy0;
    }
  }

  tft.fillScreen(BLACK);
  header("SNAKE 2P", GREEN);
  drawLevelDecor(level[0]);
  tft.drawRect(SX - 1, SY - 1, SN * SC + 1, SN * SC + 1, levelColor(level[0]));

  for (int p = 0; p < 2; p++) {
    if (!snakes[p].active) continue;
    for (int i = 0; i < snakes[p].len; i++) {
      snakeCell(snakes[p].x[i], snakes[p].y[i], i == 0 ? WHITE : snakes[p].color);
    }
  }
  foodSpawn();
  snakeTick = millis();
}

void runSnake() {
  if (longPress) { saveLevels(); drawMenu(); return; }

  for (int p = 0; p < 2; p++) {
    if (!snakes[p].active || !snakes[p].alive) continue;
    int ix = (p == 0) ? joyX : webJoyX[1];
    int iy = (p == 0) ? joyY : webJoyY[1];

    if (ix < 0 && snakes[p].dx != 1) { snakes[p].dx = -1; snakes[p].dy = 0; }
    else if (ix > 0 && snakes[p].dx != -1) { snakes[p].dx = 1; snakes[p].dy = 0; }
    else if (iy < 0 && snakes[p].dy != 1) { snakes[p].dx = 0; snakes[p].dy = -1; }
    else if (iy > 0 && snakes[p].dy != -1) { snakes[p].dx = 0; snakes[p].dy = 1; }
  }

  uint32_t now = millis();
  if (now - snakeTick < snakeDelay) return;
  snakeTick = now;

  int aliveCount = 0;
  bool anyAte = false;

  for (int p = 0; p < 2; p++) {
    if (!snakes[p].active || !snakes[p].alive) continue;

    int nx = snakes[p].x[0] + snakes[p].dx;
    int ny = snakes[p].y[0] + snakes[p].dy;

    bool hitWall = (nx < 0 || nx >= SN || ny < 0 || ny >= SN);
    bool hitSnake = snakeAnyHas(nx, ny);

    if (hitWall || hitSnake) {
      snakes[p].alive = false;
      for (int i = 0; i < snakes[p].len; i++) snakeCell(snakes[p].x[i], snakes[p].y[i], BLACK);
      playGameOverSfx();
      continue;
    }

    aliveCount++;
    bool eat = (nx == fx && ny == fy);
    if (eat) {
      if (snakes[p].len < SMAX) snakes[p].len++;
      score += 10;
      scoreDraw();
      playEatSfx();
      anyAte = true;
    } else {
      snakeCell(snakes[p].x[snakes[p].len - 1], snakes[p].y[snakes[p].len - 1], BLACK);
    }

    for (int i = snakes[p].len - 1; i > 0; i--) {
      snakes[p].x[i] = snakes[p].x[i - 1];
      snakes[p].y[i] = snakes[p].y[i - 1];
    }
    snakes[p].x[0] = nx;
    snakes[p].y[0] = ny;

    snakeCell(snakes[p].x[1], snakes[p].y[1], snakes[p].color);
    snakeCell(nx, ny, WHITE);
  }

  if (anyAte) foodSpawn();
  if (aliveCount == 0) { gameOver(); return; }
}

// ---------- 2. TETRIS ----------
constexpr int TC = 10, TR = 16, TETRIS_CELL = 8, TETRIS_X0 = 24, TETRIS_Y0 = 25;
uint8_t board[TR][TC];
const uint16_t pieceColor[] = {CYAN, YELLOW, MAGENTA, GREEN, RED, BLUE, ORANGE};
const uint16_t shapes[7][4] = {
  {0x0F00, 0x2222, 0x00F0, 0x4444},
  {0x0660, 0x0660, 0x0660, 0x0660},
  {0x0E40, 0x4C40, 0x4E00, 0x4640},
  {0x06C0, 0x8C40, 0x06C0, 0x8C40},
  {0x0C60, 0x4C80, 0x0C60, 0x4C80},
  {0x08E0, 0x6440, 0x0E20, 0x44C0},
  {0x02E0, 0x4460, 0x0E80, 0xC440}
};
int piece = 0, rot = 0, px = 3, py = 0;
uint32_t tetrisTick = 0;

bool blockAt(int p, int r, int x, int y) {
  uint16_t b = shapes[p][r & 3];
  int bit = 15 - (y * 4 + x);
  return (b >> bit) & 1;
}
bool collide(int x, int y, int r) {
  for (int by = 0; by < 4; by++)
    for (int bx = 0; bx < 4; bx++)
      if (blockAt(piece, rot + r, bx, by)) {
        int xx = x + bx, yy = y + by;
        if (xx < 0 || xx >= TC || yy >= TR) return true;
        if (yy >= 0 && board[yy][xx]) return true;
      }
  return false;
}
void drawTCell(int x, int y, uint16_t c) {
  int sx0 = TETRIS_X0 + x * TETRIS_CELL, sy0 = TETRIS_Y0 + y * TETRIS_CELL;
  tft.fillRect(sx0, sy0, TETRIS_CELL - 1, TETRIS_CELL - 1, c);
}
void drawTetrisBoard() {
  for (int y = 0; y < TR; y++)
    for (int x = 0; x < TC; x++)
      drawTCell(x, y, board[y][x] ? pieceColor[board[y][x] - 1] : BLACK);
}
void drawPieceAt(int x, int y, uint16_t c, int p, int r) {
  for (int by = 0; by < 4; by++)
    for (int bx = 0; bx < 4; bx++)
      if (blockAt(p, r, bx, by) && y + by >= 0) drawTCell(x + bx, y + by, c);
}
void spawnPiece() {
  piece = random(7); rot = 0; px = 3; py = 0;
  if (collide(px, py, 0)) { gameOver(); return; }
}
void lockPiece() {
  for (int by = 0; by < 4; by++)
    for (int bx = 0; bx < 4; bx++)
      if (blockAt(piece, rot, bx, by)) {
        int yy = py + by, xx = px + bx;
        if (yy >= 0 && yy < TR && xx >= 0 && xx < TC) board[yy][xx] = piece + 1;
      }
  int cleared = 0;
  for (int y = TR - 1; y >= 0;) {
    bool full = true;
    for (int x = 0; x < TC; x++) if (!board[y][x]) { full = false; break; }
    if (full) {
      cleared++;
      for (int yy = y; yy > 0; yy--)
        for (int x = 0; x < TC; x++) board[yy][x] = board[yy - 1][x];
      for (int x = 0; x < TC; x++) board[0][x] = 0;
    } else y--;
  }
  if (cleared) {
    score += cleared == 1 ? 100 : cleared == 2 ? 300 : cleared == 3 ? 500 : 800;
    scoreDraw();
    playEatSfx();
  }
  drawTetrisBoard();
  spawnPiece();
}
void startTetris() {
  state = TETRIS;
  bgmNoteIdx = 0;
  score = 0;
  memset(board, 0, sizeof(board));
  tft.fillScreen(BLACK);
  header("TETRIS", CYAN);
  drawLevelDecor(level[1]);
  tft.drawRect(TETRIS_X0 - 1, TETRIS_Y0 - 1, TC * TETRIS_CELL + 1, TR * TETRIS_CELL + 1, levelColor(level[1]));
  drawTetrisBoard();
  spawnPiece();
  drawPieceAt(px, py, pieceColor[piece], piece, rot);
  tetrisTick = millis();
}
void runTetris() {
  if (longPress) { saveLevels(); drawMenu(); return; }
  int ox = px, oy = py, orot = rot;
  if (joyX && millis() - lastInput > 105) {
    lastInput = millis();
    if (!collide(px + joyX, py, 0)) px += joyX;
  }
  if (joyY < 0 && millis() - lastInput > 130) {
    lastInput = millis();
    if (!collide(px, py, 1)) rot = (rot + 1) & 3;
  }
  uint32_t now = millis();
  float tocDo = tocDoGame(level[1]);
  uint32_t interval = joyY > 0 ? (uint32_t)(65.0f / tocDo) : (uint32_t)(480.0f / tocDo);
  interval = max((uint32_t)35, interval);
  if (now - tetrisTick >= interval) {
    tetrisTick = now;
    if (!collide(px, py + 1, 0)) py++;
    else { lockPiece(); return; }
  }
  if (ox != px || oy != py || orot != rot) {
    drawPieceAt(ox, oy, BLACK, piece, orot);
    drawPieceAt(px, py, pieceColor[piece], piece, rot);
  }
}

// ---------- 3. PAC-MAN ----------
constexpr int PC = 15, PR = 14, PAC_CELL = 8, PAC_X0 = 4, PAC_Y0 = 29;
const char mazeSeed[PR][PC + 1] = {
  "###############",
  "#.............#",
  "#.###.###.###.#",
  "#.............#",
  "#.###.#.#.###.#",
  "#.....#.#.....#",
  "###.#.....#.###",
  "#...#.....#...#",
  "#.###.#.#.###.#",
  "#.............#",
  "#.###.###.###.#",
  "#.............#",
  "#.............#",
  "###############"
};
char maze[PR][PC + 1];
int pacX = 1, pacY = 1, ghostX = 13, ghostY = 12;
uint32_t pacTick = 0, ghostTick = 0;

void drawMazeCell(int x, int y) {
  int xx = PAC_X0 + x * PAC_CELL, yy = PAC_Y0 + y * PAC_CELL;
  if (maze[y][x] == '#') {
    tft.fillRect(xx, yy, PAC_CELL, PAC_CELL, NAVY);
    tft.drawRect(xx, yy, PAC_CELL, PAC_CELL, BLUE);
  } else {
    tft.fillRect(xx, yy, PAC_CELL, PAC_CELL, BLACK);
    if (maze[y][x] == '.') tft.fillCircle(xx + 4, yy + 4, 1, YELLOW);
  }
}
void drawPac() {
  int xx = PAC_X0 + pacX * PAC_CELL, yy = PAC_Y0 + pacY * PAC_CELL;
  tft.fillCircle(xx + 4, yy + 4, 3, YELLOW);
  tft.fillCircle(xx + 5, yy + 2, 1, BLACK);
}
void drawGhost() {
  int xx = PAC_X0 + ghostX * PAC_CELL, yy = PAC_Y0 + ghostY * PAC_CELL;
  tft.fillCircle(xx + 4, yy + 4, 3, RED);
  tft.fillCircle(xx + 3, yy + 3, 1, WHITE);
  tft.fillCircle(xx + 6, yy + 3, 1, WHITE);
}
bool oTrong(int x, int y) {
  return x >= 0 && x < PC && y >= 0 && y < PR && maze[y][x] != '#';
}
void diChuyenGhost() {
  const int SO_O = PC * PR;
  int hangDoi[SO_O], cha[SO_O];
  bool daTham[SO_O] = {false};
  int dau = 0, cuoi = 0;
  int batDau = ghostY * PC + ghostX;
  int dich = pacY * PC + pacX;
  hangDoi[cuoi++] = batDau;
  daTham[batDau] = true;
  cha[batDau] = -1;

  const int dx[4] = {1, -1, 0, 0};
  const int dy[4] = {0, 0, 1, -1};
  while (dau < cuoi && !daTham[dich]) {
    int o = hangDoi[dau++];
    int x = o % PC, y = o / PC;
    for (int i = 0; i < 4; i++) {
      int nx = x + dx[i], ny = y + dy[i];
      if (!oTrong(nx, ny)) continue;
      int ke = ny * PC + nx;
      if (daTham[ke]) continue;
      daTham[ke] = true;
      cha[ke] = o;
      hangDoi[cuoi++] = ke;
    }
  }
  if (!daTham[dich]) return;
  int buoc = dich;
  while (cha[buoc] != batDau && cha[buoc] != -1) buoc = cha[buoc];
  if (cha[buoc] == -1) return;

  drawMazeCell(ghostX, ghostY);
  ghostX = buoc % PC;
  ghostY = buoc / PC;
  drawGhost();
}
void startPacman() {
  state = PACMAN;
  bgmNoteIdx = 0;
  score = 0;
  memcpy(maze, mazeSeed, sizeof(mazeSeed));
  pacX = 1; pacY = 1; ghostX = 13; ghostY = 12;
  tft.fillScreen(BLACK);
  header("PAC-MAN", YELLOW);
  drawLevelDecor(level[2]);
  for (int y = 0; y < PR; y++)
    for (int x = 0; x < PC; x++) drawMazeCell(x, y);
  drawPac();
  drawGhost();
  pacTick = millis();
  ghostTick = millis();
}
void runPacman() {
  if (longPress) { saveLevels(); drawMenu(); return; }
  uint32_t now = millis();
  float tocDo = tocDoGame(level[2]);
  uint32_t khoangCachBuoc = max((uint32_t)18, (uint32_t)(125.0f / tocDo));

  if ((joyX != 0 || joyY != 0) && now - pacTick >= khoangCachBuoc) {
    pacTick = now;
    int dx = (joyX != 0) ? joyX : 0;
    int dy = (joyX != 0) ? 0 : joyY;
    int nx = pacX + dx, ny = pacY + dy;
    if (oTrong(nx, ny)) {
      drawMazeCell(pacX, pacY);
      pacX = nx; pacY = ny;
      if (maze[pacY][pacX] == '.') {
        maze[pacY][pacX] = ' ';
        score += 10;
        scoreDraw();
      }
      drawPac();
    }
  }
  uint32_t tocDoDuoi = max((uint32_t)120, (uint32_t)(500.0f / tocDo));
  if (now - ghostTick >= tocDoDuoi) {
    ghostTick = now;
    diChuyenGhost();
  }
  if (pacX == ghostX && pacY == ghostY) { gameOver(); return; }

  bool conCham = false;
  for (int y = 0; y < PR && !conCham; y++)
    for (int x = 0; x < PC; x++)
      if (maze[y][x] == '.') { conCham = true; break; }
  if (!conCham) winGame();
}

// ---------- 4. BREAKOUT ----------
constexpr int BRICK_ROWS = 5, BC = 8, BW = 14, BH = 6, BX0 = 3, BY0 = 25, PADY = 145, PADW = 30;
bool bricks[BRICK_ROWS][BC];
int paddle = 49, oldPaddle = 49;
float ballx = 64, bally = 120, bvx = 1.5f, bvy = -2.0f;
int oldBallX = 64, oldBallY = 120;
uint32_t breakTick = 0;

uint16_t brickCol(int r) {
  const uint16_t c[] = {RED, ORANGE, YELLOW, GREEN, CYAN};
  return c[r];
}
void drawBrick(int r, int c) {
  int x = BX0 + c * 15, y = BY0 + r * 7;
  tft.fillRect(x, y, BW, BH, bricks[r][c] ? brickCol(r) : BLACK);
}
void startBreakout() {
  state = BREAKOUT;
  bgmNoteIdx = 0;
  score = 0;
  paddle = oldPaddle = 49;
  ballx = 64; bally = 120;
  float k = tocDoGame(level[3]);
  bvx = 1.5f * k; bvy = -2.0f * k;
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BC; c++)
      bricks[r][c] = ((r + c + level[3]) % (1 + (level[3] % 3)) != 0) || (r == 0);
  oldBallX = 64; oldBallY = 120;
  tft.fillScreen(BLACK);
  header("BREAKOUT", ORANGE);
  drawLevelDecor(level[3]);
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BC; c++) drawBrick(r, c);
  tft.fillRoundRect(paddle, PADY, PADW, 5, 2, CYAN);
  tft.fillCircle((int)ballx, (int)bally, 2, YELLOW);
  breakTick = millis();
}
void runBreakout() {
  if (longPress) { saveLevels(); drawMenu(); return; }
  uint32_t now = millis();
  if (now - breakTick < 16) return;
  breakTick = now;

  tft.fillCircle(oldBallX, oldBallY, 3, BLACK);
  tft.fillRect(oldPaddle, PADY - 1, PADW, 7, BLACK);
  oldPaddle = paddle;
  oldBallX = (int)ballx;
  oldBallY = (int)bally;

  paddle = constrain(paddle + joyX * 4, 4, 124 - PADW);
  ballx += bvx;
  bally += bvy;

  if (ballx < 4) { ballx = 4; bvx = fabsf(bvx); }
  if (ballx > 122) { ballx = 122; bvx = -fabsf(bvx); }
  if (bally < 22) { bally = 22; bvy = fabsf(bvy); }

  if (bvy > 0 && bally + 2 >= PADY && ballx >= paddle && ballx <= paddle + PADW) {
    bally = PADY - 3;
    bvy = -fabsf(bvy);
    float hit = (ballx - (paddle + PADW / 2.0f)) / (PADW / 2.0f);
    float tocDo = tocDoGame(level[3]);
    bvx = hit * 2.3f * tocDo;
    if (fabsf(bvx) < 0.3f * tocDo) bvx = bvx < 0 ? -0.3f * tocDo : 0.3f * tocDo;
    playClickSfx();
  }

  bool hit = false;
  for (int r = 0; r < BRICK_ROWS && !hit; r++)
    for (int c = 0; c < BC; c++)
      if (bricks[r][c]) {
        int x = BX0 + c * 15, y = BY0 + r * 7;
        if (ballx + 2 >= x && ballx - 2 <= x + BW && bally + 2 >= y && bally - 2 <= y + BH) {
          bricks[r][c] = false;
          drawBrick(r, c);
          score += (BRICK_ROWS - r) * 10;
          scoreDraw();
          bvy = -bvy;
          hit = true;
          playEatSfx();
          break;
        }
      }

  int remain = 0;
  for (int r = 0; r < BRICK_ROWS; r++)
    for (int c = 0; c < BC; c++) remain += bricks[r][c];
  if (!remain) { winGame(); return; }
  if (bally > H) { gameOver(); return; }

  tft.fillRoundRect(paddle, PADY, PADW, 5, 2, CYAN);
  tft.fillCircle((int)ballx, (int)bally, 2, YELLOW);
}

void startGame(int idx) {
  switch (idx) {
    case 0: startSnake(); break;
    case 1: startTetris(); break;
    case 2: startPacman(); break;
    default: startBreakout(); break;
  }
}

// =========================================================================
// SETUP & LOOP CHÍNH
// =========================================================================
void setup() {
  Serial.begin(115200);
  hwControl.begin();
  initSound();

  SPI.begin(TFT_SCLK_PIN, -1, TFT_MOSI_PIN, TFT_CS_PIN);
  tft.initR(INITR_BLACKTAB);
  tft.setRotation(0);
  tft.setSPISpeed(SPI_CLOCK);
  u8g2.begin(tft);

  randomSeed((uint32_t)hwControl.readRawX() ^ micros());
  loadSettings();
  turnOffAllWifi();

  drawMenu();
  playWinSfx();
}

void loop() {
  if (webServerRunning) {
    server.handleClient();
  }

  if (state == POWERPOINT) {
    runPowerPoint();
    return;
  }

  updateInput();
  updateBgm();

  if (state == MENU) {
    if (joyY && millis() - lastInput > 170) {
      lastInput = millis();
      int old = menuIndex;
      menuIndex = (menuIndex + (joyY > 0 ? 1 : 5)) % 6;
      drawMenuRow(old, false);
      drawMenuRow(menuIndex, true);
    }
    if (shortPress) {
      playClickSfx();
      if (menuIndex < 4) startGame(menuIndex);
      else if (menuIndex == 4) startPowerPoint();
      else if (menuIndex == 5) { settingIndex = 0; drawSettingMenu(); }
    }
  } else if (state == RESULT) {
    if (joyY && millis() - lastInput > 170) {
      lastInput = millis();
      resultChoice = 1 - resultChoice;
      drawResultChoice();
    }
    if (shortPress) {
      playClickSfx();
      if (resultChoice == 0) {
        int slot = gameSlot(lastGame);
        if (gameWon && level[slot] < MAX_LEVEL) level[slot]++;
        saveLevels();
        startGame(slot);
      } else {
        saveLevels();
        drawMenu();
      }
    }
    if (longPress) {
      saveLevels();
      drawMenu();
    }
  } else if (state == SETTING) {
    runSettingMenu();
  } else if (state == SET_WIFI) {
    runWifiMenu();
  } else if (state == SET_WIFI_SCAN) {
    runWifiScanSelect();
  } else if (state == SET_SOUND) {
    runSoundMenu();
  } else {
    switch (state) {
      case SNAKE: runSnake(); break;
      case TETRIS: runTetris(); break;
      case PACMAN: runPacman(); break;
      case BREAKOUT: runBreakout(); break;
      default: break;
    }
  }
}