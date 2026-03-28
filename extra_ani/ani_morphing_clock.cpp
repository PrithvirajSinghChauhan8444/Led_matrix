/**
 * MORPHING CLOCK (NTP WiFi Clock)
 * Digits slide/morph vertically into each other when the minute changes.
 * Connects to WiFi, syncs time via NTP.
 *
 * Standalone: Flash as src/main.cpp. Set your WiFi credentials below.
 */

#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <ESP8266WiFi.h>
#include <time.h>

#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4
#define CS_PIN 15

MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, CS_PIN, MAX_DEVICES);

// ---- WiFi Config ----
const char* WIFI_SSID = "YOUR_WIFI_SSID";
const char* WIFI_PASS = "YOUR_WIFI_PASSWORD";

// ---- Timezone (IST = UTC+5:30 = 19800 seconds) ----
const long UTC_OFFSET = 19800;

// ---- 3x5 digit font (columns, LSB = top) ----
// Each digit is 3 columns wide, 5 rows tall
const uint8_t DIGIT_FONT[10][3] = {
  {0x1F, 0x11, 0x1F}, // 0
  {0x00, 0x1F, 0x00}, // 1 (centered)
  {0x1D, 0x15, 0x17}, // 2
  {0x15, 0x15, 0x1F}, // 3
  {0x07, 0x04, 0x1F}, // 4
  {0x17, 0x15, 0x1D}, // 5
  {0x1F, 0x15, 0x1D}, // 6
  {0x01, 0x01, 0x1F}, // 7
  {0x1F, 0x15, 0x1F}, // 8
  {0x17, 0x15, 0x1F}, // 9
};

bool isInverse = true;
int intensity = 5;
int prevDigits[4] = {-1, -1, -1, -1};  // HH:MM digits
int morphOffset = 0;   // 0 = no morph, 1-5 = morphing
bool morphing = false;
int newDigits[4] = {0, 0, 0, 0};
unsigned long lastTick = 0;
bool colonBlink = false;

// Draw a 3x5 digit at a given column offset, with optional vertical shift
void drawDigit(int digit, int startCol, int yOffset, bool inv) {
  if (digit < 0 || digit > 9) return;
  for (int col = 0; col < 3; col++) {
    uint8_t colData = DIGIT_FONT[digit][col];
    for (int row = 0; row < 5; row++) {
      int drawRow = row + 1 + yOffset;  // +1 to center vertically (row 1-5 of 0-7)
      if (drawRow >= 0 && drawRow < 8) {
        bool on = (colData >> row) & 1;
        if (on) mx.setPoint(drawRow, 31 - (startCol + col), !inv);
      }
    }
  }
}

// Draw the colon between hours and minutes
void drawColon(int col, bool blink, bool inv) {
  if (!blink) return;  // colon off phase
  mx.setPoint(2, 31 - col, !inv);
  mx.setPoint(5, 31 - col, !inv);
}

void getTime(int* h, int* m) {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  *h = t->tm_hour;
  *m = t->tm_min;
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();

  // Connect WiFi
  Serial.print("Connecting to WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  // Show scrolling dots while connecting
  int dotCol = 0;
  while (WiFi.status() != WL_CONNECTED) {
    mx.setPoint(4, dotCol % 32, true);
    dotCol++;
    delay(200);
    Serial.print(".");
  }
  Serial.println(" OK!");

  // Configure NTP
  configTime(UTC_OFFSET, 0, "pool.ntp.org", "time.nist.gov");

  // Wait for time to sync
  Serial.print("Syncing time");
  time_t now = time(nullptr);
  while (now < 100000) {
    delay(500);
    Serial.print(".");
    now = time(nullptr);
  }
  Serial.println(" OK!");

  mx.clear();
}

void loop() {
  // Serial controls
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '3') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0) intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
  }

  if (millis() - lastTick < 100) return;  // ~10 FPS
  lastTick = millis();

  int h, m;
  getTime(&h, &m);

  newDigits[0] = h / 10;
  newDigits[1] = h % 10;
  newDigits[2] = m / 10;
  newDigits[3] = m % 10;

  // Detect digit changes → start morph
  if (!morphing) {
    for (int i = 0; i < 4; i++) {
      if (prevDigits[i] != newDigits[i] && prevDigits[i] >= 0) {
        morphing = true;
        morphOffset = 1;
        break;
      }
    }
  }

  // ---- Draw frame ----
  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // Clear
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 32; c++)
      mx.setPoint(r, c, isInverse);

  // Digit positions
  int digitCols[4] = {5, 10, 19, 24};

  if (morphing) {
    // Draw old digits sliding up, new digits sliding in from below
    for (int i = 0; i < 4; i++) {
      if (prevDigits[i] != newDigits[i]) {
        drawDigit(prevDigits[i], digitCols[i], -morphOffset, isInverse);     // old slides up
        drawDigit(newDigits[i], digitCols[i], 6 - morphOffset, isInverse);   // new slides in from below
      } else {
        drawDigit(newDigits[i], digitCols[i], 0, isInverse);
      }
    }

    morphOffset++;
    if (morphOffset > 6) {
      morphing = false;
      morphOffset = 0;
      for (int i = 0; i < 4; i++) prevDigits[i] = newDigits[i];
    }
  } else {
    // Static display
    for (int i = 0; i < 4; i++) {
      drawDigit(newDigits[i], digitCols[i], 0, isInverse);
      prevDigits[i] = newDigits[i];
    }
  }

  // Colon blinks every 500ms
  colonBlink = (millis() / 500) % 2 == 0;
  drawColon(16, colonBlink, isInverse);

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
