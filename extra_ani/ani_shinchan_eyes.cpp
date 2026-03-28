/**
 * SHIN-CHAN EYES ANIMATION
 * Iconic Shin-chan style: small bean-shaped eyes close together,
 * cycling through emotions.
 *
 * Emotions: Normal, Happy, Surprised, Sleepy, Angry, Sad, Mischievous
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
bool flip = false;

int emotion = 0;         // 0-6 emotion states
int emotionTimer = 0;
int blinkTimer = 0;
bool isBlinking = false;

int pX = 2, pY = 2; // For glancing around

// Emotions: 0=Normal, 1=Happy, 2=Surprised, 3=Sleepy, 4=Angry, 5=Sad, 6=Mischievous

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
  
  // Normal
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

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();
  Serial.println("Shin-chan Eyes | 3:Inverse | +/-:Brightness");
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
  flip = !flip;

  // Blink logic
  if (!isBlinking && random(100) < 5) { isBlinking = true; blinkTimer = 3; }
  if (isBlinking) { blinkTimer--; if (blinkTimer <= 0) isBlinking = false; }

  // Emotion cycling
  emotionTimer--;
  if (emotionTimer <= 0) {
    emotion = random(0, 7);  // 0-6
    emotionTimer = random(25, 60);
  }

  // Glancing
  if (random(20) > 18) { pX = random(1, 4); pY = random(1, 4); }
  int offX = pX - 2;
  int offY = pY - 2;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++) for (int c = 0; c < 32; c++) mx.setPoint(r, c, isInverse);

  drawShinEye(9,  emotion, true,  isBlinking && emotion != 1, offX, offY);
  drawShinEye(19, emotion, false, isBlinking && emotion != 1, offX, offY);
  drawMouth(emotion, offX, offY);

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
