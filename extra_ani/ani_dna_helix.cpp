/**
 * DNA HELIX ANIMATION (Inverse Style)
 * Two sine waves interleaving across 32 columns with cross-links.
 * Looks like a rotating double helix — elegant and sparse (~16 dots).
 *
 * Standalone: Flash this as src/main.cpp to run directly.
 */

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// Animation state
float phase = 0.0;
unsigned long lastUpdate = 0;
int animDelay = 90;
bool isInverse = true;

// Sine lookup — precomputed for speed (amplitude 3, center row 3.5)
// Maps angle (0-31) to row (0-7)
int sineRow(int col, float offset) {
  float angle = (col * 0.4) + offset;  // ~2 full cycles across 32 cols
  float val = sin(angle) * 3.0;        // amplitude ±3
  int row = (int)(3.5 + val + 0.5);    // center at row 3.5
  if (row < 0) row = 0;
  if (row > 7) row = 7;
  return row;
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, 5);
  mx.clear();
  Serial.println("DNA Helix | 3:Inverse | +/-:Brightness");
}

void loop() {
  // Serial controls
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '3') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') {
      int i = mx.getDeviceCount(); // just bump intensity
      // Simple intensity tracking
      static int intensity = 5;
      if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    }
    if (cmd == '-' || cmd == '_') {
      static int intensity2 = 5;
      if (intensity2 > 0) intensity2--;
      mx.control(MD_MAX72XX::INTENSITY, intensity2);
    }
  }

  if (millis() - lastUpdate < (unsigned long)animDelay) return;
  lastUpdate = millis();

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // Clear canvas
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 32; c++)
      mx.setPoint(r, c, isInverse);

  // Draw two sine strands (180° apart)
  for (int c = 0; c < 32; c++) {
    int row1 = sineRow(c, phase);
    int row2 = sineRow(c, phase + 3.14159);  // opposite strand

    // Draw strand dots
    mx.setPoint(row1, c, !isInverse);
    mx.setPoint(row2, c, !isInverse);

    // Draw cross-links every 4 columns (the "rungs" of the helix)
    if (c % 4 == 0) {
      int rMin = min(row1, row2);
      int rMax = max(row1, row2);
      // Only draw rung if strands are far enough apart
      if (rMax - rMin >= 2) {
        for (int r = rMin + 1; r < rMax; r++) {
          mx.setPoint(r, c, !isInverse);
        }
      }
    }
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);

  phase += 0.15;  // rotation speed
  if (phase > 6.28318) phase -= 6.28318;
}
