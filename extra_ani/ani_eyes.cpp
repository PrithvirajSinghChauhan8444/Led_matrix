#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15   // D8

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

const int EYE_WIDTH = 6;
const int EYE_HEIGHT = 6;
const int LEFT_EYE_COL = 6;  
const int RIGHT_EYE_COL = 20;

// Current pupil positions
int pX = 2; // Center horizontal (1=Left, 2=Center, 3=Right)
int pY = 2; // Center vertical   (1=Up,   2=Center, 3=Down)

void setup() {
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 8); 
  mx.clear();
}

void drawEye(int startC, int pupilX, int pupilY, int blinkState) {
  // Blink state 2: Fully closed
  if (blinkState == 2) {
    for (int c = 1; c < EYE_WIDTH - 1; c++) {
      mx.setPoint(3, startC + c, false);
      mx.setPoint(4, startC + c, false);
    }
    return;
  }

  for (int r = 0; r < EYE_HEIGHT; r++) {
    // Blink state 1: Squinting (shave top and bottom)
    if (blinkState == 1 && (r == 0 || r == EYE_HEIGHT - 1)) continue;

    for (int c = 0; c < EYE_WIDTH; c++) {
      // Keep 4 corners ON to make it elliptical
      if ((r == 0 && c == 0) || (r == 0 && c == EYE_WIDTH - 1) ||
          (r == EYE_HEIGHT - 1 && c == 0) || (r == EYE_HEIGHT - 1 && c == EYE_WIDTH - 1)) {
        continue; 
      }
      
      // Determine if this specific pixel is part of the 2x2 pupil
      bool isPupil = (r >= pupilY && r < pupilY + 2) && (c >= pupilX && c < pupilX + 2);
      
      // We offset 'r+1' to center the 6px eye vertically on the 8px screen
      if (isPupil) {
        mx.setPoint(r + 1, startC + c, true);  // Pupil is glowing RED
      } else {
        mx.setPoint(r + 1, startC + c, false); // Rest of eye is dark VOID
      }
    }
  }
}

void renderFace(int pupilX, int pupilY, int blinkState) {
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF); 

  // 1. Paint canvas solid RED
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < MAX_DEVICES * 8; c++) mx.setPoint(r, c, true);
  }

  // 2. Erase voids and draw pupils
  drawEye(LEFT_EYE_COL, pupilX, pupilY, blinkState);
  drawEye(RIGHT_EYE_COL, pupilX, pupilY, blinkState);

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON); 
}

void loop() {
  // 1. Check if we should blink
  if (random(10) > 7) {
    renderFace(pX, pY, 1); delay(50);
    renderFace(pX, pY, 2); delay(100);
    renderFace(pX, pY, 1); delay(50);
  }

  // 2. Pick a random direction to look (0=Center, 1=Left, 2=Right, 3=Up, 4=Down)
  int moveDir = random(5);
  
  if (moveDir == 0) { pX = 2; pY = 2; } // Center
  else if (moveDir == 1) { pX = 1; pY = 2; } // Left (1px from left boundary)
  else if (moveDir == 2) { pX = 3; pY = 2; } // Right (1px from right boundary)
  else if (moveDir == 3) { pX = 2; pY = 1; } // Up (1px from top boundary)
  else if (moveDir == 4) { pX = 2; pY = 3; } // Down (1px from bottom boundary)

  // 3. Render and hold gaze
  renderFace(pX, pY, 0);
  delay(random(600, 2000));
}