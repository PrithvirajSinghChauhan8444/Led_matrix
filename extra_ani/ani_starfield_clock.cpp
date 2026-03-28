/**
 * STARFIELD → CLOCK (Hybrid)
 * Stars drift outward from center when idle (screensaver).
 * Send 'c' via Serial to show the clock, 's' to return to starfield.
 * Auto-returns to starfield after 10 seconds of clock display.
 *
 * Standalone: Flash as src/main.cpp. Set WiFi for NTP clock.
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
const long UTC_OFFSET = 19800;  // IST

// ---- 3x5 digit font (same as morphing clock) ----
const uint8_t DIGIT_FONT[10][3] = {
  {0x1F, 0x11, 0x1F}, // 0
  {0x00, 0x1F, 0x00}, // 1
  {0x1D, 0x15, 0x17}, // 2
  {0x15, 0x15, 0x1F}, // 3
  {0x07, 0x04, 0x1F}, // 4
  {0x17, 0x15, 0x1D}, // 5
  {0x1F, 0x15, 0x1D}, // 6
  {0x01, 0x01, 0x1F}, // 7
  {0x1F, 0x15, 0x1F}, // 8
  {0x17, 0x15, 0x1F}, // 9
};

// ---- Stars ----
#define MAX_STARS 12

struct Star {
  float x, y;     // position (centered: 0,0 = center of display)
  float vx, vy;   // velocity
  bool active;
};

Star stars[MAX_STARS];

// ---- State ----
bool isInverse = true;
int intensity = 5;
unsigned long lastTick = 0;
int animDelay = 70;

enum Mode { STARFIELD, CLOCK_DISPLAY };
Mode currentMode = STARFIELD;
unsigned long clockShowTime = 0;   // when clock was activated
const unsigned long CLOCK_TIMEOUT = 10000; // 10s auto-return

void spawnStar(Star &s) {
  // Start near center with small random velocity outward
  s.x = random(-2, 3) * 0.5;
  s.y = random(-1, 2) * 0.5;
  float angle = random(0, 628) / 100.0;  // 0 to 2π
  float speed = random(3, 8) / 10.0;
  s.vx = cos(angle) * speed;
  s.vy = sin(angle) * speed * 0.5;  // slower vertically (only 8 rows)
  s.active = true;
}

void initStars() {
  for (int i = 0; i < MAX_STARS; i++) {
    spawnStar(stars[i]);
    // Spread initial positions so they don't all start at center
    stars[i].x += stars[i].vx * random(3, 15);
    stars[i].y += stars[i].vy * random(3, 15);
  }
}

void updateAndDrawStars() {
  for (int i = 0; i < MAX_STARS; i++) {
    if (!stars[i].active) {
      spawnStar(stars[i]);
      continue;
    }

    // Accelerate outward (warp effect)
    stars[i].vx *= 1.05;
    stars[i].vy *= 1.05;

    stars[i].x += stars[i].vx;
    stars[i].y += stars[i].vy;

    // Convert to screen coords (center = 16, 4)
    int sx = (int)(16 + stars[i].x);
    int sy = (int)(4 + stars[i].y);

    // Out of bounds → respawn
    if (sx < 0 || sx >= 32 || sy < 0 || sy >= 8) {
      spawnStar(stars[i]);
      continue;
    }

    mx.setPoint(sy, sx, !isInverse);
  }
}

// ---- Clock drawing ----
void drawDigit(int digit, int startCol) {
  if (digit < 0 || digit > 9) return;
  for (int col = 0; col < 3; col++) {
    uint8_t colData = DIGIT_FONT[digit][col];
    for (int row = 0; row < 5; row++) {
      int drawRow = row + 1;
      bool on = (colData >> row) & 1;
      if (on) mx.setPoint(drawRow, 31 - (startCol + col), !isInverse);
    }
  }
}

void drawClock() {
  time_t now = time(nullptr);
  struct tm* t = localtime(&now);
  int h = t->tm_hour;
  int m = t->tm_min;

  int digits[4] = { h / 10, h % 10, m / 10, m % 10 };
  int cols[4] = { 5, 10, 19, 24 };

  for (int i = 0; i < 4; i++) {
    drawDigit(digits[i], cols[i]);
  }

  // Blinking colon
  if ((millis() / 500) % 2 == 0) {
    mx.setPoint(2, 31 - 16, !isInverse);
    mx.setPoint(5, 31 - 16, !isInverse);
  }
}

void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();

  // Connect WiFi
  Serial.print("Connecting WiFi");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  int dots = 0;
  while (WiFi.status() != WL_CONNECTED) {
    mx.setPoint(4, dots % 32, true);
    dots++;
    delay(200);
    Serial.print(".");
  }
  Serial.println(" OK!");

  configTime(UTC_OFFSET, 0, "pool.ntp.org", "time.nist.gov");
  Serial.print("Syncing time");
  time_t now = time(nullptr);
  while (now < 100000) { delay(500); Serial.print("."); now = time(nullptr); }
  Serial.println(" OK!");

  initStars();
  mx.clear();
  Serial.println("Starfield Clock | c:Clock | s:Stars | 3:Inverse | +/-:Brightness");
}

void loop() {
  // Serial controls
  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == 'c' || cmd == 'C') { currentMode = CLOCK_DISPLAY; clockShowTime = millis(); }
    if (cmd == 's' || cmd == 'S') { currentMode = STARFIELD; }
    if (cmd == '3') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0) intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
  }

  // Auto-return to starfield after timeout
  if (currentMode == CLOCK_DISPLAY && (millis() - clockShowTime > CLOCK_TIMEOUT)) {
    currentMode = STARFIELD;
  }

  if (millis() - lastTick < (unsigned long)animDelay) return;
  lastTick = millis();

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);

  // Clear
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 32; c++)
      mx.setPoint(r, c, isInverse);

  if (currentMode == STARFIELD) {
    updateAndDrawStars();
  } else {
    drawClock();
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}
