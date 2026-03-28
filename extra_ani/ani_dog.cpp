/**
 * BIG DOG PET ANIMATION (Inverse Style)
 * A slightly larger pet dog that:
 * - Idle/pants (state 0)
 * - Walks (state 1)
 * - Plays with a bouncing ball (state 2)
 * - Poops (state 3)
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
int animDelay = 100;

int dogX = 12;
int dogTargetX = 12;
int dogState = 0; // 0=idle, 1=walk, 2=play, 3=poop
int dogTimer = 0;
int dogAnimFrame = 0;

int ballX = -1, ballY = -1, ballVX = 0, ballVY = 0;
int poopX = -1;

void drawDog(int x, int state, int frame) {
  auto draw = [&](int r, int c, bool on) {
    if (c >= 0 && c < 32 && r >= 0 && r < 8) mx.setPoint(r, c, on ? !isInverse : isInverse);
  };

  // Base Body (Rows 4-5)
  for(int c = x + 2; c <= x + 7; c++) draw(5, c, true);
  for(int c = x + 3; c <= x + 7; c++) draw(4, c, true);

  // Head (Rows 2-4)
  for(int r = 2; r <= 4; r++)
    for(int c = x + 7; c <= x + 9; c++)
      draw(r, c, true);
  
  // Snout
  draw(4, x + 10, true);
  if (state == 0 && frame % 2 == 0) draw(5, x + 10, true); // Panting tongue

  // Ear & shaping
  draw(2, x + 9, false); // round head
  draw(2, x + 7, true);  // ear up

  // Eye
  draw(3, x + 8, false); 
  
  // Tail (animated)
  if (state == 0 || state == 2) { // Wagging
    if (frame % 2 == 0) { draw(3, x + 1, true); draw(4, x + 2, true); }
    else                { draw(4, x + 1, true); draw(3, x + 2, true); }
  } else { // Tail down
    draw(4, x + 1, true); draw(5, x + 1, true);
  }

  // Legs & Posture
  if (state == 0) { // Sit
    draw(4, x + 2, false); // lower back drops
    draw(5, x + 2, true); 
    draw(6, x + 3, true); draw(7, x + 4, true); // back leg bent
    draw(6, x + 7, true); draw(7, x + 7, true); // front leg straight
  } 
  else if (state == 1) { // Walk
    if (frame % 2 == 0) {
      draw(6, x + 2, true); draw(7, x + 2, true);
      draw(6, x + 5, true); draw(7, x + 6, true);
      draw(6, x + 7, true); draw(7, x + 7, true);
    } else {
      draw(6, x + 3, true); draw(7, x + 4, true);
      draw(6, x + 4, true); draw(7, x + 4, true);
      draw(6, x + 8, true); draw(7, x + 9, true);
    }
  } 
  else if (state == 3) { // Poop
    draw(3, x + 3, true); draw(3, x + 4, true); // arched back
    draw(6, x + 2, true); draw(7, x + 2, true); // squat
    draw(6, x + 7, true); draw(7, x + 7, true);
  } 
  else if (state == 2) { // Play
    draw(5, x + 8, true); draw(6, x + 9, true); // front paws up
    draw(6, x + 2, true); draw(7, x + 2, true); // back legs planted
  }
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();
  Serial.println("Big Dog | 3:Inverse | +/-:Bright");
  randomSeed(analogRead(A0));
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
  dogAnimFrame++;

  // --- Dog AI Logic ---
  if (dogTimer <= 0) {
    if (dogState == 3 && poopX != -1) {
       dogState = 1; // Walk away from poop
       dogTargetX = constrain(poopX + 12, 0, 20); 
       dogTimer = 30;
    } else {
       int next = random(0, 100);
       if (next < 40) { // Walk
         dogState = 1; dogTargetX = random(0, 20); dogTimer = random(30, 80);
       } else if (next < 65) { // Play
         dogState = 2; dogTimer = random(50, 120);
         ballX = dogX + 12; if (ballX > 28) ballX = dogX - 8;
         ballY = 2; ballVX = (ballX > dogX) ? -1 : 1; ballVY = 1;
       } else if (next < 80) { // Poop
         dogState = 3; dogTimer = 25; poopX = -1;
       } else { // Idle
         dogState = 0; dogTimer = random(30, 60);
       }
    }
  }
  dogTimer--;

  // Resolve state movements
  if (dogState == 1) {
    if (dogX < dogTargetX) dogX++; else if (dogX > dogTargetX) dogX--; else dogState = 0;
  }
  if (dogState == 3 && dogTimer == 10) poopX = dogX; // Drop poop near tail

  if (dogState == 2) {
    // Ball physics
    ballX += ballVX; ballY += ballVY;
    if (ballY >= 6) { ballY = 6; ballVY = -1; } // bounce ground
    if (ballY <= 1) { ballY = 1; ballVY = 1; }  // bounce ceiling
    if (ballX <= 0 || ballX >= 30) ballVX *= -1; // wall bounce
    
    // Chase ball
    if (dogX + 9 < ballX) dogX++; else if (dogX + 7 > ballX) dogX--;
    
    // Dog snout (x+10) hits ball
    if (ballX >= dogX + 8 && ballX <= dogX + 11 && ballY >= 3) {
      ballVX = (ballX > dogX + 9) ? 1 : -1;
      ballVY = -1; // bump up
    }
  } else {
    ballX = -1;
  }

  // --- Draw ---
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++) for (int c = 0; c < 32; c++) mx.setPoint(r, c, isInverse);

  // Draw Poop
  if (poopX >= 0 && poopX < 32) {
    mx.setPoint(7, poopX, !isInverse);
    mx.setPoint(7, poopX+1, !isInverse);
    mx.setPoint(6, poopX+1, !isInverse);
  }

  // Draw Ball
  if (ballX >= 0 && ballX < 31 && ballY >= 0 && ballY < 7) {
    mx.setPoint(ballY, ballX, !isInverse);
    mx.setPoint(ballY+1, ballX, !isInverse);
    mx.setPoint(ballY, ballX+1, !isInverse);
    mx.setPoint(ballY+1, ballX+1, !isInverse);
  }

  drawDog(dogX, dogState, dogAnimFrame);
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
