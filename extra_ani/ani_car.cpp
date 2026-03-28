#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

// Hardware config
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15   // D8

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Global state for animation
int roadOffset = 0;
int speedLines[4] = {0, 8, 16, 24}; // Column positions for top speed lines
bool wheelFlicker = false;
unsigned long lastUpdate = 0;
int animationDelay = 120; // Lower = faster

// Function to draw the detailed 8-bit car
void drawInverseCar(int x) {
  // 1. Draw the wheels (Row 6)
  // We offset the wheels by 1 pixel up (row 6) from the ground (row 7)
  // They flicker to simulate spinning
  if (wheelFlicker) {
    mx.setPoint(6, x + 1, false); // Front Wheel void
    mx.setPoint(6, x + 7, false); // Rear Wheel void
  } else {
    mx.setPoint(6, x + 1, true);  // Front Wheel glowing
    mx.setPoint(6, x + 7, true);  // Rear Wheel glowing
  }

  // 2. Draw the main body (Rows 5 and 4)
  for (int c = x; c < x + 9; c++) {
    mx.setPoint(5, c, false);
    mx.setPoint(4, c, false);
  }

  // 3. Draw the cabin/roof (Rows 3 and 2)
  for (int c = x + 2; c < x + 6; c++) {
    mx.setPoint(3, c, false);
    mx.setPoint(2, c, false);
  }

  // 4. Detail: The rear antenna (Row 1)
  mx.setPoint(1, x + 8, false);
}

// Function to draw the moving road
void drawRoad(int offset) {
  for (int c = 0; c < MAX_DEVICES * 8; c++) {
    // Every 4th pixel is 'ON' to create dashes (_ _ _ _)
    bool isDash = ((c + offset) % 4 == 0);
    mx.setPoint(7, c, isDash);
  }
}

// Function to draw random "speed" particles in the background
void drawSpeedLines() {
  for (int i = 0; i < 4; i++) {
    mx.setPoint(random(0, 3), speedLines[i], false); // Draw void particle
  }
}

void setup() {
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 8); // Medium brightness
  mx.clear();
}

void loop() {
  // Only update based on the animation speed
  if (millis() - lastUpdate < animationDelay) return;
  lastUpdate = millis();

  // Draw silently in the background
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // 1. Reset Canvas: Paint it solid RED
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < MAX_DEVICES * 8; c++) mx.setPoint(r, c, true);
  }

  // 2. Update and draw the road movement
  roadOffset++;
  if (roadOffset >= 4) roadOffset = 0; // Dashes are 4px apart, so loop it
  drawRoad(roadOffset);

  // 3. Draw the detailed car void (Inverse style)
  drawInverseCar(6); // Keep car at column 6

  // 4. Update and draw the background speed particles
  for (int i = 0; i < 4; i++) {
    speedLines[i]++; // Move lines to the right
    if (speedLines[i] >= MAX_DEVICES * 8) {
      speedLines[i] = 0; // Wrap back to the left
    }
  }
  drawSpeedLines();

  // 5. Flip wheel flicker state
  wheelFlicker = !wheelFlicker;

  // Push the final frame to the LEDs
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}