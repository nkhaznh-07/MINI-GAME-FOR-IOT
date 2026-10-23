#include <Arduino.h>
#include <SPI.h>
#include <Preferences.h>
#include <math.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ST7735.h>
// MINI GAME CONSOLE V2 - ESP32-S3 + ST7735 128x160
// Pins kept from the original sketch.
constexpr int TFT_CS_PIN=10, TFT_DC_PIN=9, TFT_RST_PIN=8;
constexpr int TFT_MOSI_PIN=11, TFT_SCLK_PIN=12;
constexpr int JOY_X_PIN=4, JOY_Y_PIN=5, JOY_SW_PIN=6;
constexpr int W=128, H=160;
constexpr uint32_t SPI_CLOCK=27000000UL;
// Start at 27 MHz; lower to 10 MHz if wiring is unstable.
constexpr int SAFE_X=2, SAFE_Y=2, SAFE_W=124, SAFE_H=156;
Adafruit_ST7735 tft(TFT_CS_PIN,TFT_DC_PIN,TFT_RST_PIN);
Preferences prefs;
constexpr uint16_t BLACK=ST77XX_BLACK, WHITE=ST77XX_WHITE, RED=ST77XX_RED;
constexpr uint16_t GREEN=ST77XX_GREEN, BLUE=ST77XX_BLUE, YELLOW=ST77XX_YELLOW;
constexpr uint16_t CYAN=ST77XX_CYAN, MAGENTA=ST77XX_MAGENTA, ORANGE=0xFD20;
constexpr uint16_t GRAY=0x8410, NAVY=0x000F, PINK=0xF81F;
enum State {
    MENU, SNAKE, TETRIS, PACMAN, BREAKOUT, RESULT
};
State state=MENU, lastGame=MENU;
int gameSlot(State s);
const char* gameNames[]= {
    "SNAKE","TETRIS","PAC-MAN","BREAKOUT"
};
const uint16_t gameColors[]= {
    GREEN,CYAN,YELLOW,ORANGE
};
int menuIndex=0, resultChoice=0;
int resetPressCount=0;
uint32_t resetPressAt=0;
bool gameWon=false;
int score=0, best[4]= {
    0,0,0,0
};
// Difficulty levels are stored separately for each game (1 = easiest, 10 = hardest).
constexpr int MAX_LEVEL=10;
int level[4]= {
    1,1,1,1
};
bool newRecord=false;
uint32_t lastFrame=0, lastInput=0;
int centerX=2048, centerY=2048;
int joyX=0, joyY=0;
bool rawSW=false, stableSW=false, justDown=false, justUp=false, shortPress=false, longPress=false;
uint32_t rawChanged=0, pressAt=0;
bool longFired=false;
constexpr int DEADZONE=650;
constexpr uint32_t DEBOUNCE=28, LONG_MS=1200;
int axis(int pin,int center) {
    int d=analogRead(pin)-center;
    return abs(d)<DEADZONE ? 0 : (d<0 ? -1 : 1);
}
void updateInput() {
    uint32_t now=millis();
    joyX=axis(JOY_X_PIN,centerX);
    joyY=axis(JOY_Y_PIN,centerY);
    justDown=justUp=shortPress=longPress=false;
    bool r=(digitalRead(JOY_SW_PIN)==LOW);
    if(r!=rawSW) {
        rawSW=r;
        rawChanged=now;
    }
    if(rawSW!=stableSW && now-rawChanged>=DEBOUNCE) {
        stableSW=rawSW;
        if(stableSW) {
            justDown=true;
            pressAt=now;
            longFired=false;
        }
        else {
            justUp=true;
            if(!longFired && now-pressAt<LONG_MS)shortPress=true;
        }
    }
    if(stableSW && !longFired && now-pressAt>=LONG_MS) {
        longFired=true;
        longPress=true;
    }
}
void calibrate() {
    long sx=0,sy=0;
    for(int i=0;i<35;i++) {
        sx+=analogRead(JOY_X_PIN);
        sy+=analogRead(JOY_Y_PIN);
        delay(3);
    }
    centerX=sx/35;
    centerY=sy/35;
}
void centered(const char* s,int y,uint16_t c,uint8_t size=1) {
    tft.setTextSize(size);
    tft.setTextColor(c,BLACK);
    int16_t x1,y1;
    uint16_t tw,th;
    tft.getTextBounds(s,0,y,&x1,&y1,&tw,&th);
    tft.setCursor((W-tw)/2,y);
    tft.print(s);
}
float tocDoGame(int lv) {
    // Level 1 = 0.5; mỗi level tăng thêm 0.25.
    return 0.5f + (constrain(lv,1,MAX_LEVEL)-1)*0.25f;
}
uint16_t levelColor(int lv) {
    const uint16_t palette[10]= {
        GREEN,CYAN,YELLOW,ORANGE,MAGENTA,BLUE,RED,PINK,WHITE,0x07E0
    };
    return palette[(constrain(lv,1,MAX_LEVEL)-1)%10];
}
void drawLevelDecor(int lv) {
    // Ten distinct screen-frame patterns: one for every level.
    uint16_t c=levelColor(lv);
    tft.drawRect(2,2,124,156,c);
    int mode=(lv-1)%10;
    if(mode==0) {
        for(int x=5;x<124;x+=8)tft.drawPixel(x,23,c);
    }
    else if(mode==1) {
        for(int y=25;y<156;y+=8)tft.drawPixel(2,y,c);
    }
    else if(mode==2) {
        for(int y=25;y<156;y+=8)tft.drawPixel(125,y,c);
    }
    else if(mode==3) {
        for(int x=5;x<124;x+=8) {
            tft.drawPixel(x,22,c);
            tft.drawPixel(x,157,c);
        }
    }
    else if(mode==4) {
        tft.drawFastVLine(2,25,130,c);
        tft.drawFastVLine(125,25,130,c);
    }
    else if(mode==5) {
        for(int i=0;i<6;i++) {
            tft.drawPixel(2,30+i*20,c);
            tft.drawPixel(125,30+i*20,c);
        }
    }
    else if(mode==6) {
        for(int x=8;x<120;x+=12)tft.drawFastVLine(x,24,4,c);
    }
    else if(mode==7) {
        for(int y=28;y<155;y+=12)tft.drawFastHLine(4,y,4,c);
    }
    else if(mode==8) {
        for(int y=28;y<154;y+=6) {
            tft.drawPixel(2,y,c);
            tft.drawPixel(125,y,c);
        }
    }
    else {
        for(int i=0;i<8;i++) {
            tft.drawPixel(5+i*16,24,c);
            tft.drawPixel(5+i*16,155,c);
        }
    }
}
void header(const char* s,uint16_t c) {
    tft.fillRect(SAFE_X,SAFE_Y,SAFE_W,17,BLACK);
    tft.drawFastHLine(SAFE_X,20,SAFE_W,levelColor(level[gameSlot(state)]));
    tft.setTextSize(1);
    tft.setTextColor(c,BLACK);
    tft.setCursor(6,8);
    tft.print(s);
    tft.setTextColor(YELLOW,BLACK);
    tft.setCursor(83,8);
    tft.print("S:");
    tft.print(score);
}
void scoreDraw() {
    tft.fillRect(80,5,43,12,BLACK);
    tft.setTextSize(1);
    tft.setTextColor(YELLOW,BLACK);
    tft.setCursor(83,8);
    tft.print("S:");
    tft.print(score);
}
void drawMenuRow(int i,bool selected) {
    const char* names[]= {
        "SNAKE","TETRIS","PAC-MAN","BREAKOUT","RESET x5"
    };
    int y=30+i*24;
    uint16_t c=(i<4)?gameColors[i]:RED;
    if(selected) {
        tft.fillRoundRect(3,y,122,21,4,c);
        tft.setTextColor(BLACK,c);
    }
    else {
        tft.fillRoundRect(3,y,122,21,4,BLACK);
        tft.drawRoundRect(3,y,122,21,4,c);
        tft.setTextColor(c,BLACK);
    }
    tft.setTextSize(1);
    int16_t x1,y1;
    uint16_t tw,th;
    tft.getTextBounds(names[i],0,y,&x1,&y1,&tw,&th);
    tft.setCursor(8,y+6);
    tft.print(names[i]);
    if(i<4) {
        tft.setCursor(99,y+6);
        tft.print("L");
        tft.print(level[i]);
    }
    else {
        tft.setCursor(96,y+6);
        tft.print(resetPressCount);
        tft.print("/5");
    }
}
void drawMenu() {
    state=MENU;
    tft.fillScreen(BLACK);
    tft.drawRect(SAFE_X,SAFE_Y,SAFE_W,SAFE_H,GRAY);
    centered("MINI GAME",7,CYAN,2);
    for(int i=0;i<5;i++)drawMenuRow(i,i==menuIndex);
}
void loadBest() {
    prefs.begin("minigame",true);
    best[0]=prefs.getInt("snake",0);
    best[1]=prefs.getInt("tetris",0);
    best[2]=prefs.getInt("pacman",0);
    best[3]=prefs.getInt("breakout",0);
    level[0]=constrain(prefs.getInt("snakeLv",1),1,MAX_LEVEL);
    level[1]=constrain(prefs.getInt("tetrisLv",1),1,MAX_LEVEL);
    level[2]=constrain(prefs.getInt("pacmanLv",1),1,MAX_LEVEL);
    level[3]=constrain(prefs.getInt("breakoutLv",1),1,MAX_LEVEL);
    prefs.end();
}
const char* keyFor(State s) {
    switch(s) {
        case SNAKE:return "snake";
        case TETRIS:return "tetris";
        case PACMAN:return "pacman";
        default:return "breakout";
    }
}
int gameSlot(State s) {
    switch(s) {
        case SNAKE:return 0;
        case TETRIS:return 1;
        case PACMAN:return 2;
        default:return 3;
    }
}
void saveLevels() {
    prefs.begin("minigame",false);
    prefs.putInt("snakeLv",level[0]);
    prefs.putInt("tetrisLv",level[1]);
    prefs.putInt("pacmanLv",level[2]);
    prefs.putInt("breakoutLv",level[3]);
    prefs.end();
}
void drawResultChoice() {
    const char* opts[]= {
        gameWon ? "NEXT LEVEL  >" : "PLAY AGAIN  >","BACK TO MENU"
    };
    for(int i=0;i<2;i++) {
        int y=112+i*22;
        uint16_t edge=(i==resultChoice)?GREEN:GRAY;
        uint16_t fill=(i==resultChoice)?GREEN:BLACK;
        tft.fillRoundRect(3,y,122,19,4,fill);
        tft.drawRoundRect(3,y,122,19,4,edge);
        tft.setTextSize(1);
        tft.setTextColor((i==resultChoice)?BLACK:WHITE,fill);
        int16_t x1,y1;
        uint16_t tw,th;
        tft.getTextBounds(opts[i],0,y,&x1,&y1,&tw,&th);
        tft.setCursor((W-tw)/2,y+6);
        tft.print(opts[i]);
    }
}
void saveResult() {
    int slot=gameSlot(lastGame);
    newRecord=score>best[slot];
    if(newRecord) {
        best[slot]=score;
        prefs.begin("minigame",false);
        prefs.putInt(keyFor(lastGame),score);
        prefs.end();
    }
    state=RESULT;
    resultChoice=0;
    tft.fillScreen(BLACK);
    tft.drawRect(SAFE_X,SAFE_Y,SAFE_W,SAFE_H,GRAY);
    centered(gameWon ? "LEVEL CLEARED!" : "GAME OVER",12,WHITE,1);
    centered(gameNames[slot],25,gameColors[slot],2);
    char buf[24];
    snprintf(buf,sizeof(buf),"LEVEL: %d / %d",level[slot],MAX_LEVEL);
    centered(buf,46,CYAN,1);
    centered(gameWon ? "WELL PLAYED!" : "TRY AGAIN!",64,gameWon?GREEN:GRAY,1);
    centered("UP/DOWN + PRESS",91,GRAY,1);
    drawResultChoice();
}
void endGame() {
    lastGame=state;
    saveResult();
}
void startGame(int idx);
void gameOver() {
    gameWon=false;
    endGame();
}
void winGame() {
    gameWon=true;
    endGame();
}
// ---------- SNAKE: only redraw cells that change ----------
constexpr int SC=8, SX=4, SY=28, SN=15, SMAX=SN*SN;
int sx[SMAX],sy[SMAX],slen=3,sdx=1,sdy=0,fx=8,fy=7;
uint32_t snakeTick=0;
int snakeDelay=135;
void snakeCell(int x,int y,uint16_t c) {
    tft.fillRect(SX+x*SC,SY+y*SC,SC-1,SC-1,c);
}
bool snakeHas(int x,int y) {
    for(int i=0;i<slen;i++)if(sx[i]==x&&sy[i]==y)return true;
    return false;
}
void foodSpawn() {
    for(int n=0;n<250;n++) {
        int x=random(SN),y=random(SN);
        if(!snakeHas(x,y)) {
            fx=x;
            fy=y;
            snakeCell(fx,fy,RED);
            return;
        }
    }
}
void startSnake() {
    state=SNAKE;
    score=0;
    slen=3;
    sdx=1;
    sdy=0;
    snakeDelay=(int)(150.0f/tocDoGame(level[0]));
    sx[0]=6;
    sy[0]=7;
    sx[1]=5;
    sy[1]=7;
    sx[2]=4;
    sy[2]=7;
    tft.fillScreen(BLACK);
    header("SNAKE",GREEN);
    drawLevelDecor(level[0]);
    tft.drawRect(SX-1,SY-1,SN*SC+1,SN*SC+1,levelColor(level[0]));
    for(int i=0;i<slen;i++)snakeCell(sx[i],sy[i],i?GREEN:YELLOW);
    foodSpawn();
    snakeTick=millis();
}
void runSnake() {
    if(longPress) {
        saveLevels();
        drawMenu();
        return;
    }
    if(joyX<0&&sdx!=1) {
        sdx=-1;
        sdy=0;
    }
    else if(joyX>0&&sdx!=-1) {
        sdx=1;
        sdy=0;
    }
    else if(joyY<0&&sdy!=1) {
        sdx=0;
        sdy=-1;
    }
    else if(joyY>0&&sdy!=-1) {
        sdx=0;
        sdy=1;
    }
    uint32_t now=millis();
    if(now-snakeTick<snakeDelay)return;
    snakeTick=now;
    int nx=sx[0]+sdx,ny=sy[0]+sdy;
    bool eat=nx==fx&&ny==fy;
    if(nx<0||nx>=SN||ny<0||ny>=SN) {
        gameOver();
        return;
    }
    for(int i=0;i<slen-(eat?0:1);i++)if(sx[i]==nx&&sy[i]==ny) {
        gameOver();
        return;
    }
    if(eat) {
        if(slen<SMAX)slen++;
        score+=10;
        scoreDraw();
    }
    else snakeCell(sx[slen-1],sy[slen-1],BLACK);
    for(int i=slen-1;i>0;i--) {
        sx[i]=sx[i-1];
        sy[i]=sy[i-1];
    }
    sx[0]=nx;
    sy[0]=ny;
    snakeCell(sx[1],sy[1],GREEN);
    snakeCell(nx,ny,YELLOW);
    if(eat) {
        if(slen>=SMAX) {
            winGame();
            return;
        }
        foodSpawn();
    }
}
// ---------- TETRIS ----------
constexpr int TC=10, TR=16, TETRIS_CELL=8, TETRIS_X0=24, TETRIS_Y0=25;
uint8_t board[TR][TC];
const uint16_t pieceColor[]= {
    CYAN,YELLOW,MAGENTA,GREEN,RED,BLUE,ORANGE
};
const uint16_t shapes[7][4]= {
    {
        0x0F00,0x2222,0x00F0,0x4444
    }
    , // I
    {
        0x0660,0x0660,0x0660,0x0660
    }
    , // O
    {
        0x0E40,0x4C40,0x4E00,0x4640
    }
    , // T
    {
        0x06C0,0x8C40,0x06C0,0x8C40
    }
    , // S
    {
        0x0C60,0x4C80,0x0C60,0x4C80
    }
    , // Z
    {
        0x08E0,0x6440,0x0E20,0x44C0
    }
    , // J
    {
        0x02E0,0x4460,0x0E80,0xC440
    }
    // L
};
int piece=0,rot=0,px=3,py=0;
uint32_t tetrisTick=0;
bool softDrop=false;
bool blockAt(int p,int r,int x,int y) {
    uint16_t b=shapes[p][r&3];
    int bit=15-(y*4+x);
    return (b>>bit)&1;
}
bool collide(int x,int y,int r) {
    for(int by=0;by<4;by++)for(int bx=0;bx<4;bx++)if(blockAt(piece,rot+r,bx,by)) {
        int xx=x+bx,yy=y+by;
        if(xx<0||xx>=TC||yy>=TR)return true;
        if(yy>=0&&board[yy][xx])return true;
    }
    return false;
}
void drawTCell(int x,int y,uint16_t c) {
    int sx0=TETRIS_X0+x*TETRIS_CELL,sy0=TETRIS_Y0+y*TETRIS_CELL;
    tft.fillRect(sx0,sy0,TETRIS_CELL-1,TETRIS_CELL-1,c);
}
void drawTetrisBoard() {
    for(int y=0;y<TR;y++)for(int x=0;x<TC;x++)drawTCell(x,y,board[y][x]?pieceColor[board[y][x]-1]:BLACK);
}
void drawPieceAt(int x,int y,uint16_t c,int p,int r) {
    for(int by=0;by<4;by++)for(int bx=0;bx<4;bx++)if(blockAt(p,r,bx,by)&&y+by>=0)drawTCell(x+bx,y+by,c);
}
void spawnPiece() {
    piece=random(7);
    rot=0;
    px=3;
    py=0;
    if(collide(px,py,0)) {
        gameOver();
        return;
    }
}
void lockPiece() {
    for(int by=0;by<4;by++)for(int bx=0;bx<4;bx++)if(blockAt(piece,rot, bx,by)) {
        int yy=py+by,xx=px+bx;
        if(yy>=0&&yy<TR&&xx>=0&&xx<TC)board[yy][xx]=piece+1;
    }
    int cleared=0;
    for(int y=TR-1;y>=0;) {
        bool full=true;
        for(int x=0;x<TC;x++)if(!board[y][x]) {
            full=false;
            break;
        }
        if(full) {
            cleared++;
            for(int yy=y;yy>0;yy--)for(int x=0;x<TC;x++)board[yy][x]=board[yy-1][x];
            for(int x=0;x<TC;x++)board[0][x]=0;
        }
        else y--;
    }
    if(cleared) {
        score+=cleared==1?100:cleared==2?300:cleared==3?500:800;
        scoreDraw();
    }
    drawTetrisBoard();
    spawnPiece();
}
void startTetris() {
    state=TETRIS;
    score=0;
    memset(board,0,sizeof(board));
    tft.fillScreen(BLACK);
    header("TETRIS",CYAN);
    drawLevelDecor(level[1]);
    tft.drawRect(TETRIS_X0-1,TETRIS_Y0-1,TC*TETRIS_CELL+1,TR*TETRIS_CELL+1,levelColor(level[1]));
    drawTetrisBoard();
    spawnPiece();
    drawPieceAt(px,py,pieceColor[piece],piece,rot);
    tetrisTick=millis();
}
void runTetris() {
    if(longPress) {
        saveLevels();
        drawMenu();
        return;
    }
    int ox=px,oy=py,orot=rot;
    if(joyX && millis()-lastInput>105) {
        lastInput=millis();
        if(!collide(px+joyX,py,0))px+=joyX;
    }
    if(joyY<0&&millis()-lastInput>130) {
        lastInput=millis();
        if(!collide(px,py,1))rot=(rot+1)&3;
    }
    uint32_t now=millis();
    float tocDo=tocDoGame(level[1]);
    uint32_t interval=joyY>0 ? (uint32_t)(65.0f/tocDo) : (uint32_t)(480.0f/tocDo);
    interval=max((uint32_t)35,interval);
    if(now-tetrisTick>=interval) {
        tetrisTick=now;
        if(!collide(px,py+1,0))py++;
        else {
            lockPiece();
            return;
        }
    }
    if(ox!=px||oy!=py||orot!=rot) {
        drawPieceAt(ox,oy,BLACK,piece,orot);
        drawPieceAt(px,py,pieceColor[piece],piece,rot);
    }
}
// ---------- PAC-MAN ----------
constexpr int PC=15, PR=14, PAC_CELL=8, PAC_X0=4, PAC_Y0=29;
const char mazeSeed[PR][PC+1]= {
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
char maze[PR][PC+1];
int pacX=1,pacY=1;
int ghostX=13,ghostY=12;
uint32_t pacTick=0;
uint32_t ghostTick=0;
void drawMazeCell(int x,int y) {
    int xx=PAC_X0+x*PAC_CELL,yy=PAC_Y0+y*PAC_CELL;
    if(maze[y][x]=='#') {
        tft.fillRect(xx,yy,PAC_CELL,PAC_CELL,NAVY);
        tft.drawRect(xx,yy,PAC_CELL,PAC_CELL,BLUE);
    }
    else {
        tft.fillRect(xx,yy,PAC_CELL,PAC_CELL,BLACK);
        if(maze[y][x]=='.')tft.fillCircle(xx+4,yy+4,1,YELLOW);
    }
}
void drawPac() {
    int xx=PAC_X0+pacX*PAC_CELL;
    int yy=PAC_Y0+pacY*PAC_CELL;
    tft.fillCircle(xx+4,yy+4,3,YELLOW);
    tft.fillCircle(xx+5,yy+2,1,BLACK);
}
void drawGhost() {
    int xx=PAC_X0+ghostX*PAC_CELL;
    int yy=PAC_Y0+ghostY*PAC_CELL;
    tft.fillCircle(xx+4,yy+4,3,RED);
    tft.fillCircle(xx+3,yy+3,1,WHITE);
    tft.fillCircle(xx+6,yy+3,1,WHITE);
}
bool oTrong(int x,int y) {
    return x>=0 && x<PC && y>=0 && y<PR && maze[y][x]!='#';
}
void diChuyenGhost() {
    // Tim duong ngan nhat tu nhan vat do den Pac-Man bang BFS.
    const int SO_O=PC*PR;
    int hangDoi[SO_O];
    int cha[SO_O];
    bool daTham[SO_O]={false};
    int dau=0;
    int cuoi=0;

    int batDau=ghostY*PC+ghostX;
    int dich=pacY*PC+pacX;
    hangDoi[cuoi++]=batDau;
    daTham[batDau]=true;
    cha[batDau]=-1;

    const int dx[4]={1,-1,0,0};
    const int dy[4]={0,0,1,-1};

    while(dau<cuoi && !daTham[dich]) {
        int o=hangDoi[dau++];
        int x=o%PC;
        int y=o/PC;

        for(int i=0;i<4;i++) {
            int nx=x+dx[i];
            int ny=y+dy[i];
            if(!oTrong(nx,ny))continue;

            int ke=ny*PC+nx;
            if(daTham[ke])continue;

            daTham[ke]=true;
            cha[ke]=o;
            hangDoi[cuoi++]=ke;
        }
    }

    if(!daTham[dich])return;

    int buoc=dich;
    while(cha[buoc]!=batDau && cha[buoc]!=-1) {
        buoc=cha[buoc];
    }

    if(cha[buoc]==-1)return;

    drawMazeCell(ghostX,ghostY);
    ghostX=buoc%PC;
    ghostY=buoc/PC;
    drawGhost();
}
void startPacman() {
    state=PACMAN;
    score=0;
    memcpy(maze,mazeSeed,sizeof(mazeSeed));
    pacX=1;
    pacY=1;
    ghostX=13;
    ghostY=12;
    tft.fillScreen(BLACK);
    header("PAC-MAN",YELLOW);
    drawLevelDecor(level[2]);
    for(int y=0;y<PR;y++) {
        for(int x=0;x<PC;x++) {
            drawMazeCell(x,y);
        }
    }
    drawPac();
    drawGhost();
    pacTick=millis();
    ghostTick=millis();
}
void runPacman() {
    if(longPress) {
        saveLevels();
        drawMenu();
        return;
    }

    uint32_t now=millis();
    float tocDo=tocDoGame(level[2]);
    uint32_t khoangCachBuoc=(uint32_t)(125.0f/tocDo);
    khoangCachBuoc=max((uint32_t)18,khoangCachBuoc);

    // Pac-Man chỉ di chuyển khi joystick đang được gạt.
    if((joyX!=0 || joyY!=0) && now-pacTick>=khoangCachBuoc) {
        pacTick=now;
        int dx=0;
        int dy=0;

        if(joyX!=0) {
            dx=joyX;
        }
        else {
            dy=joyY;
        }

        int nx=pacX+dx;
        int ny=pacY+dy;

        if(oTrong(nx,ny)) {
            drawMazeCell(pacX,pacY);
            pacX=nx;
            pacY=ny;

            if(maze[pacY][pacX]=='.') {
                maze[pacY][pacX]=' ';
                score+=10;
                scoreDraw();
            }

            drawPac();
        }
    }

    // Nhân vật đỏ đuổi theo Pac-Man, tốc độ tăng theo level.
    uint32_t tocDoDuoi=(uint32_t)(500.0f/tocDo);
    tocDoDuoi=max((uint32_t)120,tocDoDuoi);
    if(now-ghostTick>=tocDoDuoi) {
        ghostTick=now;
        diChuyenGhost();
    }

    // Chạm nhân vật đỏ thì thua.
    if(pacX==ghostX && pacY==ghostY) {
        gameOver();
        return;
    }

    bool conCham=false;
    for(int y=0;y<PR;y++) {
        for(int x=0;x<PC;x++) {
            if(maze[y][x]=='.') {
                conCham=true;
                break;
            }
        }
        if(conCham)break;
    }

    if(!conCham) {
        winGame();
    }
}
// ---------- BREAKOUT ----------
constexpr int BRICK_ROWS=5,BC=8,BW=14,BH=6,BX0=3,BY0=25,PADY=145,PADW=30;
bool bricks[BRICK_ROWS][BC];
int paddle=49,oldPaddle=49;
float ballx=64,bally=120,bvx=1.5f,bvy=-2.0f;
int oldBallX=64,oldBallY=120;
uint32_t breakTick=0;
uint16_t brickCol(int r) {
    const uint16_t c[]= {
        RED,ORANGE,YELLOW,GREEN,CYAN
    };
    return c[r];
}
void drawBrick(int r,int c) {
    int x=BX0+c*15,y=BY0+r*7;
    tft.fillRect(x,y,BW,BH,bricks[r][c]?brickCol(r):BLACK);
}
void startBreakout() {
    state=BREAKOUT;
    score=0;
    paddle=oldPaddle=49;
    ballx=64;
    bally=120;
    {
        float k=tocDoGame(level[3]);
        bvx=1.5f*k;
        bvy=-2.0f*k;
    }
    for(int r=0;r<BRICK_ROWS;r++)for(int c=0;c<BC;c++)
    bricks[r][c]=((r+c+level[3])%(1+(level[3]%3))!=0) || (r==0);
    oldBallX=64;
    oldBallY=120;
    tft.fillScreen(BLACK);
    header("BREAKOUT",ORANGE);
    drawLevelDecor(level[3]);
    for(int r=0;r<BRICK_ROWS;r++)for(int c=0;c<BC;c++)drawBrick(r,c);
    tft.fillRoundRect(paddle,PADY,PADW,5,2,CYAN);
    tft.fillCircle((int)ballx,(int)bally,2,YELLOW);
    breakTick=millis();
}
void runBreakout() {
    if(longPress) {
        saveLevels();
        drawMenu();
        return;
    }
    uint32_t now=millis();
    if(now-breakTick<16)return;
    breakTick=now;
    tft.fillCircle(oldBallX,oldBallY,3,BLACK);
    tft.fillRect(oldPaddle,PADY-1,PADW,7,BLACK);
    oldPaddle=paddle;
    oldBallX=(int)ballx;
    oldBallY=(int)bally;
    paddle=constrain(paddle+joyX*4,4,124-PADW);
    ballx+=bvx;
    bally+=bvy;
    if(ballx<4) {
        ballx=4;
        bvx=fabsf(bvx);
    }
    if(ballx>122) {
        ballx=122;
        bvx=-fabsf(bvx);
    }
    if(bally<22) {
        bally=22;
        bvy=fabsf(bvy);
    }
    if(bvy>0&&bally+2>=PADY&&ballx>=paddle&&ballx<=paddle+PADW) {
        bally=PADY-3;
        bvy=-fabsf(bvy);
        float hit=(ballx-(paddle+PADW/2.0f))/(PADW/2.0f);
        float tocDo=tocDoGame(level[3]);
        bvx=hit*2.3f*tocDo;
        if(fabsf(bvx)<0.3f*tocDo) {
            bvx=bvx<0?-0.3f*tocDo:0.3f*tocDo;
        }
    }
    bool hit=false;
    for(int r=0;r<BRICK_ROWS&&!hit;r++)for(int c=0;c<BC;c++)if(bricks[r][c]) {
        int x=BX0+c*15,y=BY0+r*7;
        if(ballx+2>=x&&ballx-2<=x+BW&&bally+2>=y&&bally-2<=y+BH) {
            bricks[r][c]=false;
            drawBrick(r,c);
            score+=(BRICK_ROWS-r)*10;
            scoreDraw();
            bvy=-bvy;
            hit=true;
            break;
        }
    }
    int remain=0;
    for(int r=0;r<BRICK_ROWS;r++)for(int c=0;c<BC;c++)remain+=bricks[r][c];
    if(!remain) {
        winGame();
        return;
    }
    if(bally>H) {
        gameOver();
        return;
    }
    tft.fillRoundRect(paddle,PADY,PADW,5,2,CYAN);
    tft.fillCircle((int)ballx,(int)bally,2,YELLOW);
}
void resetEverything() {
    prefs.begin("minigame",false);
    prefs.clear();
    prefs.end();
    for(int i=0;i<4;i++) {
        best[i]=0;
        level[i]=1;
    }
    score=0;
    resetPressCount=0;
    menuIndex=0;
    gameWon=false;
    drawMenu();
}
void startGame(int idx) {
    switch(idx) {
        case 0:startSnake();
        break;
        case 1:startTetris();
        break;
        case 2:startPacman();
        break;
        default:startBreakout();
        break;
    }
}
void setup() {
    Serial.begin(115200);
    pinMode(JOY_SW_PIN,INPUT_PULLUP);
    analogReadResolution(12);
    analogSetPinAttenuation(JOY_X_PIN,ADC_11db);
    analogSetPinAttenuation(JOY_Y_PIN,ADC_11db);
    SPI.begin(TFT_SCLK_PIN,-1,TFT_MOSI_PIN,TFT_CS_PIN);
    tft.initR(INITR_BLACKTAB);
    tft.setRotation(0);
    tft.setSPISpeed(SPI_CLOCK);
    randomSeed((uint32_t)analogRead(JOY_X_PIN)^micros());
    calibrate();
    loadBest();
    rawSW=stableSW=(digitalRead(JOY_SW_PIN)==LOW);
    rawChanged=millis();
    drawMenu();
    Serial.println("Mini Game Console V2 ready.");
}
void loop() {
    updateInput();
    if(state==MENU) {
        if(joyY && millis()-lastInput>170) {
            lastInput=millis();
            int old=menuIndex;
            menuIndex=(menuIndex+(joyY>0?1:4))%5;
            drawMenuRow(old,false);
            drawMenuRow(menuIndex,true);
        }
        if(shortPress) {
            if(menuIndex==4) {
                uint32_t now=millis();
                if(now-resetPressAt>3000)resetPressCount=0;
                resetPressAt=now;
                resetPressCount++;
                drawMenuRow(4,true);
                if(resetPressCount>=5) {
                    resetEverything();
                }
            }
            else {
                resetPressCount=0;
                startGame(menuIndex);
            }
        }
    }
    else if(state==RESULT) {
        if(joyY && millis()-lastInput>170) {
            lastInput=millis();
            resultChoice=1-resultChoice;
            drawResultChoice();
        }
        if(shortPress) {
            if(resultChoice==0) {
                int slot=gameSlot(lastGame);
                if(gameWon && level[slot]<MAX_LEVEL)level[slot]++;
                saveLevels();
                startGame(slot);
            }
            else {
                saveLevels();
                drawMenu();
            }
        }
        if(longPress) {
            saveLevels();
            drawMenu();
        }
    }
    else {
        switch(state) {
            case SNAKE:runSnake();
            break;
            case TETRIS:runTetris();
            break;
            case PACMAN:runPacman();
            break;
            case BREAKOUT:runBreakout();
            break;
            default:break;
        }
    }
}
