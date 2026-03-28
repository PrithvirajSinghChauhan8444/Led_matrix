#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Hardware Config
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15 

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Settings & Modes
int currentMode = 1;      // 1:Eyes, 2:Car, 4:Pet
bool isInverse = true;    
int intensity = 5;
unsigned long lastTick = 0;
int animDelay = 80;

// Shared Animation State
bool flip = false;
int pX = 2, pY = 2;       // Eyes
int roadOff = 0;          // Car
int catX = 12, catState = 0, catTimer = 0, catTargetX = 12; // Cat

// --- 1. EYES DRAWING ---
void drawEyes(int startC, int pupilX, int pupilY, int blink) {
  if (blink == 2) return; 
  for (int r = 0; r < 6; r++) {
    if (blink == 1 && (r == 0 || r == 5)) continue;
    for (int c = 0; c < 6; c++) {
      if ((r==0&&c==0)||(r==0&&c==5)||(r==5&&c==0)||(r==5&&c==5)) continue;
      bool isPupil = (r >= pupilY && r < pupilY + 2) && (c >= pupilX && c < pupilX + 2);
      mx.setPoint(r + 1, startC + c, isPupil ? isInverse : !isInverse);
    }
  }
}

// --- 2. CAR DRAWING ---
void drawCar(int x) {
  for (int c = x; c < x + 9; c++) { mx.setPoint(4, c, !isInverse); mx.setPoint(5, c, !isInverse); }
  for (int c = x + 2; c < x + 6; c++) { mx.setPoint(2, c, !isInverse); mx.setPoint(3, c, !isInverse); }
  mx.setPoint(1, x + 8, !isInverse);
  mx.setPoint(6, x + 1, flip ? !isInverse : isInverse); // Wheels
  mx.setPoint(6, x + 7, flip ? !isInverse : isInverse);
}

// --- 4. CYBER-CAT DRAWING ---
void drawCat(int x, int state, bool flick) {
  for (int c = x+1; c < x+6; c++) mx.setPoint(6, c, !isInverse); // Body
  for (int r = 4; r < 7; r++) for (int c = x+5; c < x+9; c++) mx.setPoint(r, c, !isInverse); // Head
  mx.setPoint(3, x+5, !isInverse); mx.setPoint(3, x+8, !isInverse); // Ears
  mx.setPoint(5, x+6, isInverse);  mx.setPoint(5, x+8, isInverse);  // Eyes
  if (state == 1) { // Walking
    mx.setPoint(7, flick ? x+2:x+1, !isInverse); 
    mx.setPoint(7, flick ? x+6:x+5, !isInverse);
    mx.setPoint(flick ? 4:3, x, !isInverse); // Tail
  } else { // Sitting
    mx.setPoint(7, x+2, !isInverse); mx.setPoint(7, x+6, !isInverse);
    mx.setPoint(flick ? 4:5, x, !isInverse); // Tail flick
  }
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  Serial.println("1:Eyes | 2:Car | 4:Cat | 3:Inverse | +/-: Intensity");
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '1') currentMode = 1;
    if (cmd == '2') currentMode = 2;
    if (cmd == '4') currentMode = 4;
    if (cmd == '3') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if(intensity<15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if(intensity>0) intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    mx.clear();
  }

  if (millis() - lastTick < animDelay) return;
  lastTick = millis();
  flip = !flip;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r=0; r<8; r++) for (int c=0; c<32; c++) mx.setPoint(r, c, isInverse);

  if (currentMode == 1) {
    if (random(20) > 18) { pX = random(1, 4); pY = random(1, 4); }
    drawEyes(6, pX, pY, (random(50) > 48) ? 1 : 0);
    drawEyes(20, pX, pY, (random(50) > 48) ? 1 : 0);
  } 
  else if (currentMode == 2) {
    roadOff = (roadOff + 1) % 4;
    for (int c=0; c<32; c++) if ((c+roadOff)%4==0) mx.setPoint(7, c, !isInverse);
    drawCar(6);
  }
  else if (currentMode == 4) {
    if (catTimer <= 0) {
      catState = random(0, 2); 
      catTimer = random(20, 60);
      if (catState == 1) catTargetX = random(0, 23);
    }
    catTimer--;
    if (catState == 1) {
      if (catX < catTargetX) catX++; else if (catX > catTargetX) catX--; else catState = 0;
    }
    drawCat(catX, catState, flip);
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}