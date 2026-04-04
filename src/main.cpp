#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h> // Changed for ESP32
#include <time.h>

// ==================== HARDWARE CONFIG ====================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// Custom one-sided ESP32 pins
#define DATA_PIN 13  // D13 on the board
#define CLK_PIN  14  // D14 on the board
#define CS_PIN   27  // D27 on the board

// Custom software SPI constructor
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// ==================== WiFi CONFIG ====================
const char* WIFI_SSID = "OhhMarr";
const char* WIFI_PASS = "omaromar";
const long  UTC_OFFSET = 19800;  // IST = UTC+5:30
bool wifiConnected = false;

// ==================== GLOBAL STATE ====================
int  currentMode = 1;
bool isInverse   = true;
int  intensity   = 5;
unsigned long lastTick = 0;
int  animDelay   = 80;
bool flip        = false;

// --- Eyes state ---
int pX = 2, pY = 2;
int eyeState = 0; // 0=Open, 1=Closing, 2=Closed, 3=Opening
int eyeTimer = 0;

// --- Car state ---
int roadOff = 0;

// --- Cat state ---
int catX = 8, catState = 0, catTargetX = 5, catTimer = 0, catDir = 1, catAnimFrame = 0;

// --- Shin-chan state ---
int shinEmotion = 0, shinEmotionTimer = 0, shinBlinkTimer = 0;
bool shinIsBlinking = false;

// --- Slime Pet state (Mode 3 / s) ---
int slimeX = 15, slimeY = 7, slimeState = 0, slimeTimer = 0, slimeDir = 1, slimeAnim = 0;

// --- Stickman Action (Mode 8) ---
int gruntX = 15, gruntState = 0, gruntTimer = 0, gruntDir = 1, gruntFrame = 0;

// --- Dino Fight (Mode 13 / e) ---
struct DinoFighter { int x, state, timer, hp; };
DinoFighter df1 = {3, 0, 0, 5};
DinoFighter df2 = {28, 0, 0, 5};
struct Fireball { int x, y, vx; bool active; int type; };
Fireball fb = {0,0,0,false, 0};

// --- Train state ---
int trainX = 32;

// --- Dog state ---
int dogX = 12, dogTargetX = 12, dogState = 0, dogTimer = 0, dogAnimFrame = 0, dogDir = 1;
int ballX = -1, ballY = -1, ballVX = 0, ballVY = 0, poopX = -1;

// --- Clock state ---
const uint8_t DIGIT_FONT[10][3] = {
  {0x1F, 0x11, 0x1F}, {0x00, 0x1F, 0x00}, {0x1D, 0x15, 0x17},
  {0x15, 0x15, 0x1F}, {0x07, 0x04, 0x1F}, {0x17, 0x15, 0x1D},
  {0x1F, 0x15, 0x1D}, {0x01, 0x01, 0x1F}, {0x1F, 0x15, 0x1F},
  {0x17, 0x15, 0x1F},
};
int  prevDigits[4] = {-1, -1, -1, -1};
int  morphOffset   = 0;
bool morphing      = false;

// ==================== 1. EYES ====================
void drawEyes(int startC, int offsetX, int offsetY, int blink) {
  if (blink == 2) return;
  for (int r = 0; r < 6; r++) {
    if (blink == 1 && (r == 0 || r == 5)) continue;
    for (int c = 0; c < 6; c++) {
      if ((r==0&&c==0)||(r==0&&c==5)||(r==5&&c==0)||(r==5&&c==5)) continue;
      bool isPupil = (r >= 2 && r < 4) && (c >= 2 && c < 4);
      int drawR = r + 1 + offsetY;
      int drawC = startC + c + offsetX;
      if (drawR >= 0 && drawR < 8 && drawC >= 0 && drawC < 32)
        mx.setPoint(drawR, drawC, isPupil ? isInverse : !isInverse);
    }
  }
}

void animEyes() {
  // 1. Handle State Transitions
  if (eyeTimer <= 0) {
    if (eyeState == 0) { 
      // Currently open. Pick next action: Blink, Move, or Idle.
      int r = random(100);
      if (r < 15) { 
        eyeState = 1;     // Start blink sequence
        eyeTimer = 1;     // Squint for 1 frame (~80ms)
      } else if (r < 40) {
        pX = random(1, 4); // Pick new direction
        pY = random(1, 4);
        eyeTimer = random(10, 25); // Stare in new direction for 0.8s to 2s
      } else {
        eyeTimer = random(5, 10); // Just idle and hold current stare
      }
    } 
    else if (eyeState == 1) { // Was squinting closed
      eyeState = 2; 
      eyeTimer = 1; // Fully closed for 1 frame
    } 
    else if (eyeState == 2) { // Was fully closed
      eyeState = 3; 
      eyeTimer = 1; // Squint opening for 1 frame
    } 
    else if (eyeState == 3) { // Was squinting open
      eyeState = 0; 
      // THE FIX: The Recovery Pause! 
      // Force the eyes to stay open and stare in the original direction 
      // for at least 4-10 frames before allowing a new movement.
      eyeTimer = random(4, 10); 
    }
  } else {
    eyeTimer--;
  }

  // 2. Map the current state to the drawing variable
  int blinkDraw = 0; // Default wide open
  if (eyeState == 1 || eyeState == 3) blinkDraw = 1; // Squinting
  else if (eyeState == 2) blinkDraw = 2;             // Fully closed

  // 3. Draw both eyes
  drawEyes(6,  pX - 2, pY - 2, blinkDraw);
  drawEyes(20, pX - 2, pY - 2, blinkDraw);
}

// ==================== 2. CAR (Sleek Sports Car) ====================
void drawCar(int x) {
  for (int c = x; c < x + 14; c++) { mx.setPoint(4, c, !isInverse); mx.setPoint(5, c, !isInverse); }
  for (int c = x + 4; c < x + 10; c++) mx.setPoint(3, c, !isInverse);
  for (int c = x + 5; c < x + 9; c++) mx.setPoint(2, c, !isInverse);
  mx.setPoint(3, x + 12, !isInverse);
  mx.setPoint(4, x + 14, !isInverse);
  mx.setPoint(2, x, !isInverse); mx.setPoint(1, x, !isInverse);
  mx.setPoint(4, x + 14, flip ? !isInverse : isInverse);
  mx.setPoint(4, x, flip ? !isInverse : isInverse);
  mx.setPoint(6, x + 2, !isInverse); mx.setPoint(6, x + 3, !isInverse);
  mx.setPoint(6, x + 10, !isInverse); mx.setPoint(6, x + 11, !isInverse);
  if (flip) {
    mx.setPoint(5, x + 2, isInverse); mx.setPoint(5, x + 11, isInverse); 
  }
}

void animCar() {
  roadOff = (roadOff + 1) % 4;
  for (int c = 0; c < 32; c++) if ((c + roadOff) % 4 == 0) mx.setPoint(7, c, !isInverse);
  drawCar(5);  
}

// ==================== 4. CAT ====================
void drawCat(int x, int state, int frame, int dir) {
  auto draw = [&](int r, int relC, bool on) {
    int drawC = x + (relC * dir);
    if (drawC >= 0 && drawC < 32 && r >= 0 && r < 8) 
      mx.setPoint(r, drawC, on ? !isInverse : isInverse);
  };
  for(int r = 3; r <= 5; r++) for(int c = 4; c <= 7; c++) draw(r, c, true);
  draw(2, 4, true); draw(2, 7, true); 
  if (state == 2) { 
    draw(4, 5, false); draw(4, 6, false); 
  } else { draw(4, 6, false); }          
  
  if (state == 0 || state == 2) { 
    for(int r = 5; r <= 7; r++) for(int c = 0; c <= 4; c++) draw(r, c, true); 
    draw(7, -1, true); draw(6, -1, true); draw(5, -2, true); 
    draw(7, 4, false); 
    if (state == 2) { 
       if (frame % 4 < 2) draw(1, 8, true); else draw(0, 9, true); 
    }
  } else if (state == 1) { 
    for(int r = 4; r <= 5; r++) for(int c = -1; c <= 4; c++) draw(r, c, true);
    if (frame % 2 == 0) { draw(3, -2, true); draw(2, -2, true); }
    else { draw(3, -3, true); draw(2, -3, true); }
    if (frame % 2 == 0) { draw(6, -1, true); draw(7, -1, true); draw(6, 2, true); draw(7, 3, true); } 
    else { draw(6, 0, true); draw(7, 0, true); draw(6, 3, true); draw(7, 2, true); }
  } else if (state == 3) { 
    for(int r = 4; r <= 5; r++) for(int c = -2; c <= 3; c++) draw(r, c, true);
    draw(6, -1, true); draw(7, 0, true);
    if (frame % 2 == 0) { draw(4, 8, true); draw(4, 9, true); }
    else { draw(5, 8, true); draw(6, 9, true); }
    draw(3, -3, true); draw(3, -4, true);
  }
}

void animCat() {
  catAnimFrame++;
  if (catTimer <= 0) {
    int next = random(0, 100);
    if (next < 30) { catState = 1; catTargetX = random(3, 28); catTimer = random(30, 70); }
    else if (next < 60) { catState = 0; catTimer = random(40, 80); } 
    else if (next < 85) { catState = 2; catTimer = random(50, 100); } 
    else { catState = 3; catTimer = 20; } 
  }
  catTimer--;
  
  if (catState == 1) {
    if (catX < catTargetX) { catX++; catDir = 1; }
    else if (catX > catTargetX) { catX--; catDir = -1; }
    else catState = 0;
  } else if (catState == 3) { 
    if (catTimer % 5 == 0) {
      if (catX > 15) catDir = -1; else if (catX < 10) catDir = 1;
      catX += (catDir * 1);
    }
  }
  drawCat(catX, catState, catAnimFrame, catDir);
}

// ==================== 3. SLIME PET (mode 3 / s) ====================
void drawSlime(int x, int y, int state, int frame) {
  auto draw = [&](int r, int c, bool on) {
    if (c >= 0 && c < 32 && r >= 0 && r < 8) mx.setPoint(r, c, on ? !isInverse : isInverse);
  };
  
  if (state == 0) { 
    draw(y, x-4, true); draw(y, x+4, true);
    for(int c=x-3; c<=x+3; c++) { draw(y, c, true); draw(y-1, c, true); }
    for(int c=x-2; c<=x+2; c++) { draw(y-2, c, true); draw(y-3, c, true); }
    for(int c=x-1; c<=x+1; c++) { draw(y-4, c, true); }
    
    draw(y-1, x-3, false); draw(y-1, x+3, false);
    if (frame % 40 < 4) { 
       draw(y-2, x-2, false); draw(y-2, x+2, false);
    } else { 
       draw(y-2, x-2, false); draw(y-2, x+2, false);
       draw(y-3, x-1, false); draw(y-3, x+1, false);
    }
  } else if (state == 1 || state == 3) { 
    draw(y, x-5, true); draw(y, x+5, true); 
    for(int c=x-4; c<=x+4; c++) { draw(y, c, true); draw(y-1, c, true); }
    for(int c=x-3; c<=x+3; c++) draw(y-2, c, true);
    for(int c=x-1; c<=x+1; c++) draw(y-3, c, true);
    draw(y-1, x-2, false); draw(y-1, x+2, false);
  } else if (state == 2) { 
    draw(y, x-2, true); draw(y, x+2, true);
    for(int c=x-1; c<=x+1; c++) { draw(y, c, true); draw(y-1, c, true); }
    for(int c=x-2; c<=x+2; c++) { draw(y-2, c, true); draw(y-3, c, true); draw(y-4, c, true); }
    draw(y-5, x-1, true); draw(y-5, x, true); draw(y-5, x+1, true);
    draw(y-3, x-1, false); draw(y-3, x+1, false);
  }
}

void animSlime() {
  slimeAnim++;
  if (slimeTimer <= 0) {
    if (slimeState == 0) {
      if (random(100) < 30) { slimeState = 1; slimeTimer = 2; }
      else { slimeTimer = random(20, 50); }
    } else if (slimeState == 1) {
      slimeState = 2; slimeTimer = 10;
      slimeDir = (random(2)==0)?1:-1; 
    } else if (slimeState == 2) {
      slimeState = 3; slimeTimer = 3; slimeY = 7;
    } else if (slimeState == 3) {
      slimeState = 0; slimeTimer = random(20, 40);
    }
  } else { slimeTimer--; }

  if (slimeState == 2) { 
    int phase = 10 - slimeTimer; 
    if (phase < 5) slimeY--; else slimeY++;
    slimeX += slimeDir;
    slimeX = constrain(slimeX, 5, 26);
  }
  drawSlime(slimeX, slimeY, slimeState, slimeAnim);
}

// ==================== 7. MORPHING CLOCK ====================
void drawDigit(int digit, int startCol, int yOff) {
  if (digit < 0 || digit > 9) return;
  for (int col = 0; col < 3; col++) {
    uint8_t d = DIGIT_FONT[digit][col];
    for (int row = 0; row < 5; row++) {
      int dr = row + 1 + yOff;
      if (dr >= 0 && dr < 8 && ((d >> row) & 1))
        mx.setPoint(dr, 31 - (startCol + col), !isInverse);  
    }
  }
}

void drawColon() {
  if ((millis() / 500) % 2 == 0) {
    mx.setPoint(2, 31 - 16, !isInverse);  
    mx.setPoint(5, 31 - 16, !isInverse);
  }
}

void getTimeHM(int* h, int* m) {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  *h = t->tm_hour; *m = t->tm_min;
}

void animMorphClock() {
  if (!wifiConnected) {
    for (int c = 4; c < 28; c += 2) mx.setPoint(4, c, !isInverse);
    return;
  }
  int h, m;
  getTimeHM(&h, &m);
  int nd[4] = { h/10, h%10, m/10, m%10 };
  int cols[4] = { 5, 10, 19, 24 };  

  if (!morphing) {
    for (int i = 0; i < 4; i++) {
      if (prevDigits[i] != nd[i] && prevDigits[i] >= 0) { morphing = true; morphOffset = 1; break; }
    }
  }

  if (morphing) {
    for (int i = 0; i < 4; i++) {
      if (prevDigits[i] != nd[i]) {
        drawDigit(prevDigits[i], cols[i], -morphOffset);
        drawDigit(nd[i], cols[i], 6 - morphOffset);
      } else {
        drawDigit(nd[i], cols[i], 0);
      }
    }
    morphOffset++;
    if (morphOffset > 6) {
      morphing = false; morphOffset = 0;
      for (int i = 0; i < 4; i++) prevDigits[i] = nd[i];
    }
  } else {
    for (int i = 0; i < 4; i++) { drawDigit(nd[i], cols[i], 0); prevDigits[i] = nd[i]; }
  }
  drawColon();
}

// ==================== 8. STICKMAN ACTION ====================
void drawGrunt(int x, int frame, int dir, int state) {
  auto draw = [&](int relR, int relC, bool on) {
    int dc = x + (relC*dir); 
    if(dc>=0 && dc<32 && relR>=0 && relR<8) mx.setPoint(relR, dc, on?!isInverse:isInverse);
  };
  
  for(int c=-1;c<=1;c++) { draw(0,c,true); draw(4,c,true); }
  draw(1,-2,true); draw(2,-2,true); draw(3,-2,true);
  draw(1,2,true);  draw(2,2,true);  draw(3,2,true);
  
  for(int r=1;r<=3;r++) for(int c=-1;c<=1;c++) draw(r,c,true);

  draw(1,0,false); 
  draw(2,-1,false); draw(2,0,false); draw(2,1,false); 
  draw(3,0,false);

  if (state == 0) { 
    draw(5,0,true); draw(6,0,true); 
    if (frame%2==0) { draw(5, 2, true); draw(6, -2, true); }
    else { draw(6, 2, true); draw(5, -2, true); }
    if (frame%2==0) { draw(7,-1,true); draw(7,2,true); }
    else { draw(7,1,true); draw(7,-2,true); }
  } else if (state == 1) { 
    draw(5,-1,true); draw(6,-2,true); 
    draw(4, 3, true); draw(5, 3, true); 
    draw(5,-3, true); 
    draw(7,-2,true); draw(7,2,true); 
  }
}

void animStickAction() {
  gruntFrame++;
  if (gruntTimer <= 0) {
    int r = random(100);
    if (r < 30) { gruntState = 1; gruntTimer = random(5, 15); }
    else if (r < 60) { gruntState = 0; gruntTimer = random(20, 50); gruntDir = (random(2)==0)?1:-1; }
    else { gruntState = 0; gruntTimer = random(20, 50); }
  } else {
    gruntTimer--;
  }
  
  if (gruntState == 0 && gruntTimer % 2 == 0) {
     if (random(10)>3) gruntX += gruntDir;
  }
  gruntX = constrain(gruntX, 3, 28);
  
  drawGrunt(gruntX, gruntFrame, gruntDir, gruntState);
}

// ==================== 9. LARGE BULLET TRAIN ====================
void drawTrain(int x) {
  auto draw = [&](int r, int c, bool on) {
    if (c >= 0 && c < 32 && r >= 0 && r < 8) mx.setPoint(r, c, on ? !isInverse : isInverse);
  };
  for(int c=x; c<=x+31; c++) { draw(7, c, true); draw(6, c, true); }
  draw(5, x+1, true); draw(5, x+2, true);
  draw(4, x+2, true); draw(4, x+3, true);
  draw(3, x+3, true); draw(3, x+4, true);
  draw(2, x+4, true); draw(2, x+5, true);
  draw(1, x+5, true); draw(1, x+6, true);
  for(int c=x+6; c<=x+31; c++) draw(1, c, true);
  for(int r=2; r<=5; r++) {
    int startC = x + 6 - (r-1);
    for(int c=startC; c<=x+31; c++) draw(r, c, true);
  }
  for(int c=x+7; c <= x+28; c++) {
     if ((c-(x+7)) % 8 < 6) { draw(3, c, false); draw(4, c, false); }
  }
  draw(2, x+15, false); draw(3, x+15, false); draw(4, x+15, false); draw(5, x+15, false); 
  draw(2, x+24, false); draw(3, x+24, false); draw(4, x+24, false); draw(5, x+24, false); 
}

void animTrain() {
  for (int c = 0; c < 32; c++) if (c % 4 != 0) mx.setPoint(7, c, !isInverse);
  drawTrain(trainX);
  trainX -= 2;
  if (trainX < -32) trainX = 32;
}

// ==================== 10. SHIN-CHAN EYES (mode 0) ====================
void drawShinEye(int startC, int emotion, bool isLeft, bool blink, int offsetX, int offsetY) {
  auto draw = [&](int r, int c, bool on) {
    int drawR = r - 2 + offsetY;
    int drawC = c + offsetX;
    if (drawR >= 0 && drawR < 8 && drawC >= 0 && drawC < 32)
      mx.setPoint(drawR, drawC, on ? !isInverse : isInverse);
  };
  if (emotion == 1) {
    draw(3, startC + 1, true); draw(3, startC + 2, true);
    draw(4, startC,     true); draw(4, startC + 3, true);
    return;
  }
  if (blink) {
    for (int c = startC; c < startC + 4; c++) draw(4, c, true);
    return;
  }
  if (emotion == 3) {
    for (int c = startC; c < startC + 4; c++) draw(5, c, true);
    draw(4, startC, true); draw(4, startC + 3, true);
    draw(5, startC + 1, false);
    return;
  }
  if (emotion == 2) {
    for (int c = startC; c < startC + 4; c++) {
      draw(2, c, true); draw(3, c, true);
      draw(4, c, true); draw(5, c, true);
    }
    draw(3, startC + 1, false); draw(3, startC + 2, false);
    draw(4, startC + 1, false); draw(4, startC + 2, false);
    draw(4, startC + 2, true);
    return;
  }
  if (emotion == 6 && !isLeft) {
    for (int c = startC; c < startC + 4; c++) draw(4, c, true);
    draw(3, startC + 3, true); draw(5, startC + 3, true);
    return;
  }
  for (int c = startC; c < startC + 4; c++) {
    draw(3, c, true); draw(4, c, true); draw(5, c, true);
  }
  draw(3, startC, false); draw(3, startC + 3, false);
  draw(5, startC, false); draw(5, startC + 3, false);
  draw(3, startC + 1, false); draw(3, startC + 2, false);
  draw(4, startC + 1, false);
}

void drawMouth(int emotion, int offsetX, int offsetY) {
  auto draw = [&](int r, int c, bool on) {
    int drawR = r - 2 + offsetY;
    int drawC = c + offsetX;
    if (drawR >= 0 && drawR < 8 && drawC >= 0 && drawC < 32)
      mx.setPoint(drawR, drawC, on ? !isInverse : isInverse);
  };
  int cx = 15;
  if (emotion == 1) {
    draw(7, cx - 2, true); draw(7, cx + 1, true);
    draw(6, cx - 1, true); draw(6, cx,     true);
  } else if (emotion == 2) {
    draw(6, cx, true); draw(6, cx - 1, true);
    draw(7, cx, true); draw(7, cx - 1, true);
  } else if (emotion == 5) {
    draw(6, cx - 2, true); draw(6, cx + 1, true);
    draw(7, cx - 1, true); draw(7, cx,     true);
  } else if (emotion == 6) {
    draw(7, cx - 1, true); draw(7, cx, true); draw(6, cx + 1, true);
  } else {
    draw(7, cx - 1, true); draw(7, cx, true);
  }
}

void animShinEyes() {
  if (!shinIsBlinking && random(100) < 5) { shinIsBlinking = true; shinBlinkTimer = 3; }
  if (shinIsBlinking) { shinBlinkTimer--; if (shinBlinkTimer <= 0) shinIsBlinking = false; }
  shinEmotionTimer--;
  if (shinEmotionTimer <= 0) { shinEmotion = random(0, 7); shinEmotionTimer = random(25, 60); }
  
  if (random(20) > 18) { pX = random(1, 4); pY = random(1, 4); }
  int offX = pX - 2;
  int offY = pY - 2;

  drawShinEye(9,  shinEmotion, true,  shinIsBlinking && shinEmotion != 1, offX, offY);
  drawShinEye(19, shinEmotion, false, shinIsBlinking && shinEmotion != 1, offX, offY);
  drawMouth(shinEmotion, offX, offY);
}

// ==================== 12. DOG REDESIGN (mode a) ====================
void drawDog(int x, int state, int frame, int dir) {
  auto draw = [&](int r, int relC, bool on) {
    int drawC = x + (relC * dir);
    if (drawC >= 0 && drawC < 32 && r >= 0 && r < 8) mx.setPoint(r, drawC, on ? !isInverse : isInverse);
  };
  
  // Head
  draw(3, 5, true); draw(3, 6, true); draw(3, 7, true);
  draw(4, 5, true); draw(4, 6, true); draw(4, 7, true); draw(4, 8, true); 
  draw(2, 6, true); 
  draw(3, 8, false); 
  if (state == 0 && frame % 2 == 0) draw(5, 8, true); 
  
  // Floppy ear
  if (frame % 2 == 0) { draw(3, 4, true); draw(4, 4, true); }
  else { draw(4, 4, true); draw(5, 4, true); }
  
  // Body
  for(int r=5; r<=6; r++) for(int c=1; c<=6; c++) draw(r, c, true);
  
  // Tail
  if (state == 0 || state == 2 || state == 4) { 
    if (frame % 2 == 0) { draw(3, 0, true); draw(4, 0, true); }
    else                { draw(4, 0, true); draw(5, -1, true); }
  } else { draw(4, 0, true); }

  // Legs & Context
  if (state == 0) { 
    draw(6, 1, false); draw(7, 2, true); draw(7, 5, true); draw(7, 6, true);
  } else if (state == 1) { 
    if (frame % 2 == 0) { draw(7, 2, true); draw(7, 5, true); }
    else { draw(7, 1, true); draw(7, 6, true); }
  } else if (state == 4) { 
    draw(6, 1, false); draw(7, 2, true); draw(7, 5, true);
    if (frame % 4 < 2) draw(1, 9, true); else draw(0, 10, true);
  } else if (state == 5) { 
    draw(7, 1, true); draw(7, 3, true); 
    if (frame % 2 == 0) { draw(6, 9, true); draw(7, 10, true); }
    else { draw(5, 10, true); draw(7, 9, true); }
  } else { 
    draw(7, 2, true); draw(7, 5, true);
  }
}

void animDog() {
  dogAnimFrame++;
  if (dogTimer <= 0) {
    if (dogState == 3 && poopX != -1) {
       dogState = 1; dogTargetX = constrain(poopX + 12, 0, 20); dogTimer = 30;
    } else {
       int next = random(0, 100);
       if (next < 30) { dogState = 1; dogTargetX = random(0, 20); dogTimer = random(30, 80); }
       else if (next < 50) {
         dogState = 2; dogTimer = random(50, 120);
         ballX = dogX + (dogDir * 12); if (ballX > 28 || ballX < 0) ballX = dogX - (dogDir * 8);
         ballY = 2; ballVX = (ballX > dogX) ? -1 : 1; ballVY = 1;
       } else if (next < 60) { dogState = 3; dogTimer = 25; poopX = -1; }
       else if (next < 75) { dogState = 4; dogTimer = random(40, 100); }
       else if (next < 85) { dogState = 5; dogTimer = random(30, 60); }
       else { dogState = 0; dogTimer = random(30, 60); }
    }
  }
  dogTimer--;

  if (dogState == 1) { 
    if (dogX < dogTargetX) { dogX++; dogDir = 1; } 
    else if (dogX > dogTargetX) { dogX--; dogDir = -1; } 
    else dogState = 0; 
  }
  if (dogState == 3 && dogTimer == 10) poopX = dogX - (dogDir * 2);
  if (dogState == 2) {
    if (ballX > dogX + 5) dogDir = 1; else if (ballX < dogX - 5) dogDir = -1;
    ballX += ballVX; ballY += ballVY;
    if (ballY >= 6) { ballY = 6; ballVY = -1; }
    if (ballY <= 1) { ballY = 1; ballVY = 1; } 
    if (ballX <= 0 || ballX >= 30) ballVX *= -1;
    int mouthX = dogX + (dogDir * 9);
    if (mouthX < ballX) dogX++; else if (mouthX > ballX) dogX--;
    if (abs(ballX - mouthX) <= 2 && ballY >= 3) { ballVX = (ballX > dogX) ? 1 : -1; ballVY = -1; }
  } else { ballX = -1; }

  if (poopX >= 0 && poopX < 32) {
    mx.setPoint(7, poopX, !isInverse); mx.setPoint(7, poopX+1, !isInverse); mx.setPoint(6, poopX+1, !isInverse);
  }
  if (ballX >= 0 && ballX < 31 && ballY >= 0 && ballY < 7) {
    mx.setPoint(ballY, ballX, !isInverse); mx.setPoint(ballY+1, ballX, !isInverse);
    mx.setPoint(ballY, ballX+1, !isInverse); mx.setPoint(ballY+1, ballX+1, !isInverse);
  }
  drawDog(dogX, dogState, dogAnimFrame, dogDir);
}



// ==================== 13. DINO FIGHT (mode e) ====================
void drawDinoFighter(DinoFighter &d, bool isLeft) {
  int dir = isLeft ? 1 : -1;
  int bx = d.x;
  auto draw = [&](int relR, int relC, bool on) {
    int dc = bx + (relC * dir);
    if(dc>=0 && dc<32 && relR>=0 && relR<8) mx.setPoint(relR, dc, on?!isInverse:isInverse);
  };
  
  if (d.state == 4) { 
    for(int c=-1; c<=1; c++) { draw(4,c,true); draw(5,c,true); draw(6,c,true); } 
    draw(3,0,true); 
    for(int c=-2; c<=2; c++) draw(7,c,true); 
    draw(5,0,false); draw(5,-1,false); draw(5,1,false); draw(4,0,false); draw(6,0,false); 
    return;
  }
  
  if (d.state == 2) { 
    for (int c=0; c<=3; c++) { draw(4,c,true); draw(5,c,true); }
    draw(5,4,true); draw(6,4,true); draw(7,1,true); draw(7,3,true);
    draw(4,4,true); draw(4,5,true); draw(4,6,true);
    draw(5,5,true); draw(5,6,true);
  } else {
    for(int c=1; c<=5; c++) { draw(0,c,true); draw(1,c,true); draw(2,c,true); } 
    draw(1,2,false); 
    if (d.state == 1 || d.state == 3) { draw(3,4,true); draw(3,5,true); } 
    else { for (int c=1; c<=5; c++) draw(3,c,true); } 
    
    draw(4,-3,true); 
    for(int c=-2; c<=4; c++) draw(4,c,true);
    for(int c=-2; c<=3; c++) draw(5,c,true);
    for(int c=-1; c<=2; c++) draw(6,c,true);
    draw(7,0,true); draw(7,2,true); 
  }
}

void animDinoFight() {
  if (fb.active) {
    fb.x += fb.vx;
    int hitR = (fb.type == 1) ? 3 : 1; 
    for(int i=0; i<hitR; i++) {
        if(fb.y+i >= 0 && fb.y+i < 8 && fb.x >= 0 && fb.x < 32)
            mx.setPoint(fb.y + i, fb.x, !isInverse);
    }
    
    if (fb.vx > 0 && fb.x >= df2.x - 2) {
      if (df2.state != 2 && df2.state != 4) {
        df2.hp--; if (df2.hp <= 0) { df2.state = 4; df1.state = 5; } 
      }
      fb.active = false;
    } else if (fb.vx < 0 && fb.x <= df1.x + 2) {
      if (df1.state != 2 && df1.state != 4) {
        df1.hp--; if (df1.hp <= 0) { df1.state = 4; df2.state = 5; } 
      }
      fb.active = false;
    }
    if (fb.x < -4 || fb.x > 36) fb.active = false;
  } else {
    df1.timer--; df2.timer--;
    
    auto tickState = [&](DinoFighter &df, DinoFighter &opp, int dir) {
      if (df.state == 4 || df.state == 5) return; 
      if (df.timer <= 0) {
        int r = random(100);
        if (r < 5) { df.state = 3; df.timer = 15; fb = {df.x+(5*dir), 2, 2*dir, true, 1}; } 
        else if (r < 20) { df.state = 1; df.timer = 10; fb = {df.x+(5*dir), 2, 2*dir, true, 0}; } 
        else if (r < 40) { df.state = 0; df.timer = 10; df.x += dir; } 
        else if (r < 60) { df.state = 0; df.timer = 10; df.x -= dir; } 
        else { df.state = 0; df.timer = random(10,30); }
      }
      if (dir == 1) df.x = constrain(df.x, 2, opp.x - 4);
      else df.x = constrain(df.x, opp.x + 4, 29);
    };
    
    tickState(df1, df2, 1);
    tickState(df2, df1, -1);
  }

  if (fb.active && fb.vx > 0 && fb.x > df2.x - 14 && df2.state == 0) { df2.state = 2; df2.timer = 15; }
  if (fb.active && fb.vx < 0 && fb.x < df1.x + 14 && df1.state == 0) { df1.state = 2; df1.timer = 15; }

  if (df1.state != 0 && df1.state != 4 && df1.state != 5 && df1.timer <= 0) df1.state = 0;
  if (df2.state != 0 && df2.state != 4 && df2.state != 5 && df2.timer <= 0) df2.state = 0;

  drawDinoFighter(df1, true);
  drawDinoFighter(df2, false);
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();

  Serial.print("WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long wifiStart = millis();
  int dot = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    mx.setPoint(4, dot % 32, true);
    dot++;
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    configTime(UTC_OFFSET, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println(" OK!");
    time_t now = time(nullptr);
    unsigned long ntpStart = millis();
    while (now < 100000 && millis() - ntpStart < 5000) { delay(300); now = time(nullptr); }
    Serial.println("NTP synced!");
  } else {
    Serial.println(" SKIP (no WiFi — modes 7,8 limited)");
  }
  mx.clear();

  Serial.println("--- LED MATRIX CONTROLLER ---");
  Serial.println("0:ShinEyes| 1:Eyes | 2:Car | 3/s:Slime | 4:Cat");
  Serial.println("7:Clock | 8:Stickman | 9:BulletTrain | a:Dog | e:DinoFight");
  Serial.println("i:Inverse | +/-:Brightness");
}

// ==================== MAIN LOOP ====================
void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '0') currentMode = 0;
    if (cmd == '1') currentMode = 1;
    if (cmd == '2') currentMode = 2;
    if (cmd == '3' || cmd == 's' || cmd == 'S') { currentMode = 3; slimeState = 0; slimeY = 7; }
    if (cmd == '4') currentMode = 4;
    if (cmd == '7') currentMode = 7;
    if (cmd == '8') { currentMode = 8; gruntX = 15; gruntState = 0; }
    if (cmd == '9') { currentMode = 9; trainX = 32; }
    if (cmd == 'a' || cmd == 'A') { currentMode = 12; dogState = 0; ballX = -1; poopX = -1; }
    if (cmd == 'e' || cmd == 'E') { currentMode = 13; df1.state = 0; df2.state = 0; fb.active = false; df1.hp = 5; df2.hp = 5; }
    if (cmd == 'i' || cmd == 'I') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0)  intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    mx.clear();
  }

  if (millis() - lastTick < (unsigned long)animDelay) return;
  lastTick = millis();
  flip = !flip;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++) for (int c = 0; c < 32; c++) mx.setPoint(r, c, isInverse);

  switch (currentMode) {
    case 0: animShinEyes();  break;
    case 1: animEyes();      break;
    case 2: animCar();       break;
    case 3: animSlime();     break;
    case 4: animCat();       break;
    case 7: animMorphClock();break;
    case 8: animStickAction();break;
    case 9: animTrain();     break;
    case 12: animDog();      break;
    case 13: animDinoFight();break;
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}