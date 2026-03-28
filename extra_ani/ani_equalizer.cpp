/**
 * EQUALIZER / VU METER ANIMATION (Inverse Style)
 * 8 vertical bars bouncing to random heights, simulating music.
 * Each bar is 2 cols wide with 2-col gaps. Peak dot lingers at top.
 *
 * Standalone: Flash as src/main.cpp to run.
 * Optional: Connect analog mic on A0 for real audio reactivity.
 */

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

#define NUM_BARS 8
#define BAR_WIDTH 2
#define BAR_SPACING 2  // 2px bar + 2px gap = 4px per bar, 8 bars = 32px

// Bar state
int barHeight[NUM_BARS]   = {0};
int barTarget[NUM_BARS]   = {0};
int peakHeight[NUM_BARS]  = {0};
int peakHold[NUM_BARS]    = {0};  // frames to hold peak before falling

// Config
bool isInverse = true;
int intensity = 5;
unsigned long lastTick = 0;
int animDelay = 60;
bool useAnalog = false;  // Set true if mic on A0

// Smoothly move bars toward their targets
void updateBars() {
  for (int i = 0; i < NUM_BARS; i++) {
    // Generate new random target periodically
    if (random(100) < 30) {
      if (useAnalog) {
        // Read mic and map to bar range
        int raw = analogRead(A0);
        barTarget[i] = map(raw, 0, 1024, 0, 8);
      } else {
        // Random bounce for visual effect
        barTarget[i] = random(1, 9);  // 1-8 height
      }
    }

    // Smooth rise/fall
    if (barHeight[i] < barTarget[i]) {
      barHeight[i] += 1;  // rise fast
    } else if (barHeight[i] > barTarget[i]) {
      barHeight[i] -= 1;  // fall slower
    }

    // Clamp
    if (barHeight[i] < 0) barHeight[i] = 0;
    if (barHeight[i] > 8) barHeight[i] = 8;

    // Peak tracking
    if (barHeight[i] >= peakHeight[i]) {
      peakHeight[i] = barHeight[i];
      peakHold[i] = 12;  // hold peak for 12 frames
    } else {
      if (peakHold[i] > 0) {
        peakHold[i]--;
      } else {
        if (peakHeight[i] > 0) peakHeight[i]--;
      }
    }
  }
}

void drawBars() {
  for (int i = 0; i < NUM_BARS; i++) {
    int startCol = i * (BAR_WIDTH + BAR_SPACING);

    // Draw the filled bar (from bottom row 7 going up)
    for (int h = 0; h < barHeight[i]; h++) {
      int row = 7 - h;
      for (int w = 0; w < BAR_WIDTH; w++) {
        mx.setPoint(row, startCol + w, !isInverse);
      }
    }

    // Draw the peak dot (single row, lingers at the top)
    if (peakHeight[i] > 0 && peakHeight[i] <= 8) {
      int peakRow = 7 - peakHeight[i] + 1;
      if (peakRow < 0) peakRow = 0;
      for (int w = 0; w < BAR_WIDTH; w++) {
        mx.setPoint(peakRow, startCol + w, !isInverse);
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();
  Serial.println("Equalizer | A:Toggle Analog Mic | 3:Inverse | +/-:Brightness");

  // Seed random
  randomSeed(analogRead(A0));
}

void loop() {
  // Serial controls
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '3') isInverse = !isInverse;
    if (cmd == 'a' || cmd == 'A') { useAnalog = !useAnalog; Serial.println(useAnalog ? "Analog ON" : "Analog OFF"); }
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0) intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
  }

  if (millis() - lastTick < (unsigned long)animDelay) return;
  lastTick = millis();

  updateBars();

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // Clear canvas
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 32; c++)
      mx.setPoint(r, c, isInverse);

  drawBars();

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
