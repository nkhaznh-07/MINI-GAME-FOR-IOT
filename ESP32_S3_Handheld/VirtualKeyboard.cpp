#include "VirtualKeyboard.h"

VirtualKeyboard::VirtualKeyboard() {
    currentMode = MODE_LOWERCASE;
    drawKeyCallback = nullptr;
    cursorRow = 0;
    cursorCol = 0;
}

void VirtualKeyboard::begin(int width, int height, int startY,
                           void (*drawFunc)(int x, int y, int w, int h, const char* label, bool isSpecial, bool isPressed)) {
    screenWidth = width;
    screenHeight = height;
    kbY = startY;
    kbHeight = height - startY;
    drawKeyCallback = drawFunc;
    cursorRow = 0;
    cursorCol = 0;
}

void VirtualKeyboard::setMode(KeyMode mode) {
    currentMode = mode;
    draw();
}

void VirtualKeyboard::resetCursor() {
    cursorRow = 0;
    cursorCol = 0;
}

static int getColsInRow(int row) {
    if (row == 0 || row == 1) return 10;
    if (row == 2) return 9;
    if (row == 3) return 9; // Shift + 7 ký tự + Backspace
    return 5;               // 123/ABC + ',' + Space + '.' + Enter
}

void VirtualKeyboard::moveCursor(int dx, int dy) {
    if (dy != 0) {
        cursorRow = (cursorRow + dy + 5) % 5;
        int maxCols = getColsInRow(cursorRow);
        if (cursorCol >= maxCols) cursorCol = maxCols - 1;
    }
    if (dx != 0) {
        int maxCols = getColsInRow(cursorRow);
        cursorCol = (cursorCol + dx + maxCols) % maxCols;
    }
}

void VirtualKeyboard::draw() {
    if (!drawKeyCallback) return;

    int rowHeight = kbHeight / 5;
    int margin = 1;
    int w10 = (screenWidth - (11 * margin)) / 10;

    // Hàng 1 (10 phím số)
    const char* r1_lower[] = {"1","2","3","4","5","6","7","8","9","0"};
    for (int i = 0; i < 10; i++) {
        int x = margin + i * (w10 + margin);
        int y = kbY + 0 * rowHeight + margin;
        bool sel = (cursorRow == 0 && cursorCol == i);
        drawKeyCallback(x, y, w10, rowHeight - margin * 2, r1_lower[i], false, sel);
    }

    // Hàng 2 (10 phím)
    const char* r2_lower[] = {"q","w","e","r","t","y","u","i","o","p"};
    const char* r2_upper[] = {"Q","W","E","R","T","Y","U","I","O","P"};
    const char* r2_num[]   = {"[","]","{","}","#","%","^","*","+","="};
    const char* r2_sym[]   = {"_","\\","|","~","<",">","^","*","+","="};
    for (int i = 0; i < 10; i++) {
        int x = margin + i * (w10 + margin);
        int y = kbY + 1 * rowHeight + margin;
        const char* label = r2_lower[i];
        if (currentMode == MODE_UPPERCASE) label = r2_upper[i];
        else if (currentMode == MODE_NUMBERS) label = r2_num[i];
        else if (currentMode == MODE_SYMBOLS) label = r2_sym[i];
        bool sel = (cursorRow == 1 && cursorCol == i);
        drawKeyCallback(x, y, w10, rowHeight - margin * 2, label, false, sel);
    }

    // Hàng 3 (9 phím - căn giữa)
    const char* r3_lower[] = {"a","s","d","f","g","h","j","k","l"};
    const char* r3_upper[] = {"A","S","D","F","G","H","J","K","L"};
    const char* r3_num[]   = {"-","/",":",";","(",")","$","&","@"};
    int offset9 = (screenWidth - (9 * w10 + 8 * margin)) / 2;
    for (int i = 0; i < 9; i++) {
        int x = offset9 + i * (w10 + margin);
        int y = kbY + 2 * rowHeight + margin;
        const char* label = r3_lower[i];
        if (currentMode == MODE_UPPERCASE) label = r3_upper[i];
        else if (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) label = r3_num[i];
        bool sel = (cursorRow == 2 && cursorCol == i);
        drawKeyCallback(x, y, w10, rowHeight - margin * 2, label, false, sel);
    }

    // Hàng 4 (Shift + 7 phím + Backspace)
    int y4 = kbY + 3 * rowHeight + margin;
    int wSpecial = (screenWidth - (7 * w10 + 8 * margin)) / 2;

    const char* shiftLbl = (currentMode == MODE_NUMBERS) ? "#+=" :
                           (currentMode == MODE_SYMBOLS) ? "123" : "^";
    bool shiftSel = (cursorRow == 3 && cursorCol == 0) || (currentMode == MODE_UPPERCASE);
    drawKeyCallback(margin, y4, wSpecial, rowHeight - margin * 2, shiftLbl, true, shiftSel);

    const char* r4_lower[] = {"z","x","c","v","b","n","m"};
    const char* r4_upper[] = {"Z","X","C","V","B","N","M"};
    const char* r4_num[]   = {".",",","?","!","'","\"","~"};
    for (int i = 0; i < 7; i++) {
        int x = margin + wSpecial + margin + i * (w10 + margin);
        const char* label = r4_lower[i];
        if (currentMode == MODE_UPPERCASE) label = r4_upper[i];
        else if (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) label = r4_num[i];
        bool sel = (cursorRow == 3 && cursorCol == (i + 1));
        drawKeyCallback(x, y4, w10, rowHeight - margin * 2, label, false, sel);
    }

    int xDelete = screenWidth - margin - wSpecial;
    bool delSel = (cursorRow == 3 && cursorCol == 8);
    drawKeyCallback(xDelete, y4, wSpecial, rowHeight - margin * 2, "<X", true, delSel);

    // Hàng 5 (123/ABC, ',', Space, '.', OK)
    int y5 = kbY + 4 * rowHeight + margin;
    int wMode  = (int)(w10 * 1.8f);
    int wEnter = (int)(w10 * 2.0f);
    int wComma = w10;
    int wDot   = w10;
    int wSpace = screenWidth - (wMode + wComma + wDot + wEnter + 6 * margin);

    const char* modeLabel = (currentMode == MODE_NUMBERS || currentMode == MODE_SYMBOLS) ? "ABC" : "123";
    drawKeyCallback(margin, y5, wMode, rowHeight - margin * 2, modeLabel, true, (cursorRow == 4 && cursorCol == 0));
    drawKeyCallback(margin * 2 + wMode, y5, wComma, rowHeight - margin * 2, ",", false, (cursorRow == 4 && cursorCol == 1));
    drawKeyCallback(margin * 3 + wMode + wComma, y5, wSpace, rowHeight - margin * 2, "SPACE", false, (cursorRow == 4 && cursorCol == 2));
    drawKeyCallback(margin * 4 + wMode + wComma + wSpace, y5, wDot, rowHeight - margin * 2, ".", false, (cursorRow == 4 && cursorCol == 3));
    drawKeyCallback(margin * 5 + wMode + wComma + wSpace + wDot, y5, wEnter, rowHeight - margin * 2, "OK", true, (cursorRow == 4 && cursorCol == 4));
}

String VirtualKeyboard::pressSelectedKey() {
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

String VirtualKeyboard::handleTouch(int touchX, int touchY) {
    if (touchY < kbY || touchY > screenHeight) return "";
    int rowHeight = kbHeight / 5;
    int row = (touchY - kbY) / rowHeight;
    int margin = 2;
    int w10 = (screenWidth - (11 * margin)) / 10;

    if (row == 0) {
        int idx = (touchX - margin) / (w10 + margin);
        if (idx >= 0 && idx < 10) { cursorRow = 0; cursorCol = idx; return pressSelectedKey(); }
    } else if (row == 1) {
        int idx = (touchX - margin) / (w10 + margin);
        if (idx >= 0 && idx < 10) { cursorRow = 1; cursorCol = idx; return pressSelectedKey(); }
    } else if (row == 2) {
        int offset9 = (screenWidth - (9 * w10 + 8 * margin)) / 2;
        int idx = (touchX - offset9) / (w10 + margin);
        if (idx >= 0 && idx < 9) { cursorRow = 2; cursorCol = idx; return pressSelectedKey(); }
    } else if (row == 3) {
        int wSpecial = (screenWidth - (7 * w10 + 8 * margin)) / 2;
        if (touchX < wSpecial + margin) { cursorRow = 3; cursorCol = 0; return pressSelectedKey(); }
        if (touchX > screenWidth - wSpecial - margin) { cursorRow = 3; cursorCol = 8; return "\b"; }
        int idx = (touchX - (margin + wSpecial + margin)) / (w10 + margin);
        if (idx >= 0 && idx < 7) { cursorRow = 3; cursorCol = idx + 1; return pressSelectedKey(); }
    } else if (row == 4) {
        int wMode = (int)(w10 * 1.8f);
        int wEnter = (int)(w10 * 2.0f);
        if (touchX < wMode + margin) { cursorRow = 4; cursorCol = 0; return pressSelectedKey(); }
        if (touchX > screenWidth - wEnter - margin) return "\n";
        return " ";
    }
    return "";
}