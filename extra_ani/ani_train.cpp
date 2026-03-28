/**
 * BULLET TRAIN ANIMATION
 * Sleek, high-speed train zooming across static tracks.
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
int animDelay = 70;

int trainX = 32;

void drawTrain(int x) {
  auto draw = [&](int r, int c, bool on) {
    if (c >= 0 && c < 32 && r >= 0 && r < 8) mx.setPoint(r, c, on ? !isInverse : isInverse);
  };
  
  // Nose (x to x+3)
  draw(6, x, true); // lower lip
  draw(5, x+1, true); draw(6, x+1, true);
  draw(4, x+2, true); draw(5, x+2, true); draw(6, x+2, true);
  draw(4, x+3, true); draw(5, x+3, true); draw(6, x+3, true); 
  
  // Cockpit window
  for(int c=x+4; c <= x+6; c++) draw(4, c, false);
  for(int c=x+4; c <= x+6; c++) { draw(5, c, true); draw(6, c, true); }
  
  // Main body
  for(int c=x+7; c <= x+31; c++) {
    draw(4, c, true); draw(5, c, true); draw(6, c, true);
    if ((c - (x+7)) % 3 == 0) draw(4, c, false); // windows
  }
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();
  Serial.println("Bullet Train | i:Inverse | +/-:Brightness");
}

void loop() {
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'i' || cmd == 'I') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0) intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
  }

  if (millis() - lastTick < (unsigned long)animDelay) return;
  lastTick = millis();

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++) for (int c = 0; c < 32; c++) mx.setPoint(r, c, isInverse);

  // Static Tracks
  for (int c = 0; c < 32; c++) {
    if (c % 4 != 0) mx.setPoint(7, c, !isInverse);
  }

  drawTrain(trainX);
  trainX -= 2; // Fast movement
  if (trainX < -32) trainX = 32;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
