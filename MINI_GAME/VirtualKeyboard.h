#ifndef VIRTUAL_KEYBOARD_H
#define VIRTUAL_KEYBOARD_H

#include <Arduino.h>

enum KeyMode {
    MODE_LOWERCASE,
    MODE_UPPERCASE,
    MODE_NUMBERS,
    MODE_SYMBOLS
};

enum KeyType {
    KEY_CHAR,
    KEY_SHIFT,
    KEY_BACKSPACE,
    KEY_MODE_NUM,
    KEY_MODE_SYM,
    KEY_MODE_ABC,
    KEY_SPACE,
    KEY_ENTER
};

struct Key {
    int x, y, w, h;
    const char* labelLower;
    const char* labelUpper;
    const char* labelNum;
    const char* labelSym;
    KeyType type;
};

class VirtualKeyboard {
private:
    KeyMode currentMode;
    int screenWidth;
    int screenHeight;
    int kbY;
    int kbHeight;

    // Con trỏ phục vụ điều khiển bằng Joystick
    int cursorRow;
    int cursorCol;

    void (*drawKeyCallback)(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed);

public:
    VirtualKeyboard();

    void begin(int width, int height, int startY,
               void (*drawFunc)(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed));

    void draw();
    KeyMode getMode() { return currentMode; }
    void setMode(KeyMode mode);
    String handleTouch(int touchX, int touchY);

    // Các hàm mở rộng cho Joystick (không làm ảnh hưởng code cảm ứng cũ)
    void resetCursor();
    void moveCursor(int dx, int dy);
    String pressSelectedKey();
};

#endif