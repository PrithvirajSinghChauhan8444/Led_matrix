/**
 * CYBER-CAT ANIMATION
 * It walks, sits, sleeps, and scratches!
 * Facing direction mirrors dynamically.
 *
 * Standalone: Flash as src/main.cpp to run directly.
 */

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

bool isInverse = true;
int intensity = 5;
unsigned long lastTick = 0;
int animDelay = 120;

int catX = 8, catState = 0, catTargetX = 5, catTimer = 0, catDir = 1, catAnimFrame = 0;

void drawCat(int x, int state, int frame, int dir) {
  auto draw = [&](int r, int relC, bool on) {
    int drawC = x + (relC * dir);
    if (drawC >= 0 && drawC < 32 && r >= 0 && r < 8) 
      mx.setPoint(r, drawC, on ? !isInverse : isInverse);
  };
  for(int r = 3; r <= 5; r++) for(int c = 4; c <= 7; c++) draw(r, c, true);
  draw(2, 4, true); draw(2, 7, true); // Ears
  if (state == 2) { 
    draw(4, 5, false); draw(4, 6, false); // closed eyes
  } else { draw(4, 6, false); }          // open eye
  
  if (state == 0 || state == 2) { 
    for(int r = 5; r <= 7; r++) for(int c = -1; c <= 4; c++) draw(r, c, true);
    draw(7, -2, true); draw(6, -2, true); draw(5, -3, true); // Tail wrapped
    draw(7, 4, false); // paw notch
    if (state == 2) { // Sleep bubble
       if (frame % 4 < 2) draw(1, 8, true); else draw(0, 9, true); 
    }
  } else if (state == 1) { // Walk
    for(int r = 4; r <= 5; r++) for(int c = -2; c <= 4; c++) draw(r, c, true);
    if (frame % 2 == 0) { draw(3, -3, true); draw(2, -3, true); }
    else { draw(3, -4, true); draw(2, -4, true); }
    if (frame % 2 == 0) { draw(6, -2, true); draw(7, -2, true); draw(6, 2, true); draw(7, 3, true); } 
    else { draw(6, -1, true); draw(7, -1, true); draw(6, 3, true); draw(7, 2, true); }
  } else if (state == 3) { // Pounce
    for(int r = 4; r <= 5; r++) for(int c = -3; c <= 3; c++) draw(r, c, true);
    draw(6, -2, true); draw(7, -1, true);
    if (frame % 2 == 0) { draw(4, 8, true); draw(4, 9, true); }
    else { draw(5, 8, true); draw(6, 9, true); }
    draw(3, -4, true); draw(3, -5, true);
  }
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();
  Serial.println("Cyber-Cat | 3:Inverse | +/-:Brightness");
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '3') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0) intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
  }

  if (millis() - lastTick < (unsigned long)animDelay) return;
  lastTick = millis();
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

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++) for (int c = 0; c < 32; c++) mx.setPoint(r, c, isInverse);

  drawCat(catX, catState, catAnimFrame, catDir);

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
