#include <Arduino.h>
#include <ArduinoJson.h>
#include <AsyncTCP.h>
#include <ESPAsyncWebServer.h>
#include <HTTPClient.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h> // Changed for ESP32
#include <Preferences.h>
#include <stdint.h>
#include <time.h>

// ==================== HARDWARE CONFIG ====================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// Custom one-sided ESP32 pins
#define DATA_PIN 13 // D13 on the board
#define CLK_PIN 14  // D14 on the board
#define CS_PIN 27   // D27 on the board

// Custom software SPI constructor
MD_MAX72XX mx =
    MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// ==================== WiFi CONFIG ====================
const char *WIFI_SSID = "OhhMarr";
const char *WIFI_PASS = "omaromar";
const long UTC_OFFSET = 19800; // IST = UTC+5:30
bool wifiConnected = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
String networkText = "";

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>EMO CONTROL</title>
    <style>
        :root { --bg: #000; --fg: #fff; --accent: #fff; --gray: #222; }
        body { background: var(--bg); color: var(--fg); font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, Arial, sans-serif; margin: 0; padding: 2rem; display: flex; flex-direction: column; align-items: center; min-height: 100vh; }
        .container { width: 100%; max-width: 400px; border: 1px solid var(--gray); padding: 2rem; border-radius: 4px; box-sizing: border-box; }
        h1 { font-size: 1rem; letter-spacing: 0.4rem; margin-bottom: 2.5rem; font-weight: 300; text-align: center; text-transform: uppercase; }
        .section { margin-bottom: 2rem; }
        .section-title { font-size: 0.65rem; text-transform: uppercase; color: #555; margin-bottom: 1rem; letter-spacing: 0.15rem; border-bottom: 1px solid #111; padding-bottom: 0.5rem; }
        .grid { display: grid; grid-template-columns: 1fr 1fr; gap: 0.75rem; }
        button { background: transparent; color: var(--fg); border: 1px solid var(--gray); padding: 0.9rem; cursor: pointer; font-size: 0.75rem; transition: all 0.2s; border-radius: 2px; text-transform: uppercase; letter-spacing: 0.05rem; }
        button:hover { background: var(--fg); color: var(--bg); border-color: var(--fg); }
        button:active { transform: scale(0.96); opacity: 0.8; }
        .full { grid-column: span 2; }
        .status { margin-top: 2rem; font-size: 0.6rem; color: #333; text-align: center; letter-spacing: 0.1rem; }
    </style>
    <script>
        function set(path, val) { fetch(`/${path}?set=${val}`); }
        function cmd(c) { fetch(`/cmd?c=${encodeURIComponent(c)}`); }
    </script>
</head>
<body>
    <div class="container">
        <h1>EMOBOT / CORE</h1>
        <div class="section">
            <div class="section-title">System Modes</div>
            <div class="grid">
                <button onclick="set('mode','2')">Eyes</button>
                <button onclick="set('mode','a')">Dog</button>
                <button onclick="set('mode','15')">Game</button>
                <button onclick="set('mode','99')">Text</button>
            </div>
        </div>
        <div class="section">
            <div class="section-title">Expression</div>
            <div class="grid">
                <button onclick="set('mood',16)">Stars</button>
                <button onclick="set('mood',17)">Dance</button>
                <button onclick="set('mood',18)">Sing</button>
                <button onclick="set('mood','auto')">Auto</button>
            </div>
        </div>
        <div class="section">
            <div class="section-title">Hardware</div>
            <div class="grid">
                <button onclick="cmd('+')">Light +</button>
                <button onclick="cmd('-')">Light -</button>
                <button class="full" onclick="cmd('i')">Invert Signal</button>
            </div>
        </div>
        <div class="status">V2.4 / CONNECTION STABLE</div>
    </div>
</body>
</html>
)rawliteral";

// ==================== GLOBAL STATE ====================
// NOTE: These are written by AsyncWebServer (FreeRTOS task) and read by loop().
// `volatile` prevents the compiler from caching them in registers across task
// switches.
volatile int currentMode = 2; // Default to Robo Eyes
volatile bool isInverse = true;
volatile int intensity = 0; // Lowest brightness
unsigned long lastTick = 0;
int animDelay = 80;
bool flip = false;

// --- Stats & Orientation (New) ---
Preferences prefs;
String cpuUsage = "0%", ramUsage = "0G", tempValue = "0C", battUsage = "0%";
unsigned long lastStatCycle = 0;
int currentStatIndex = 0;
bool flipH = false, flipV = false;
int rotation = 0; // 0, 90, 180, 270

// Shout feature
String shoutMsg = "";
int shoutSpeed = 50;
unsigned long shoutEndTime = 0;
int shoutModeOld = 2;

// 8x8 Bitmaps
const uint8_t ICON_CPU[] = {0xBD, 0x81, 0xBD, 0x81, 0xBD, 0x81, 0xBD, 0x00};
const uint8_t ICON_RAM[] = {0xFF, 0x81, 0xBD, 0xBD, 0xBD, 0xBD, 0x81, 0xFF};
const uint8_t ICON_TEMP[] = {0x18, 0x18, 0x18, 0x18, 0x18, 0x3C, 0x3C, 0x18};
const uint8_t ICON_MSG[] = {0xFF, 0x81, 0xC3, 0xA5, 0x99, 0x81, 0xFF, 0x00};
const uint8_t ICON_BATT[] = {0x3C, 0x3C, 0xFF, 0x81, 0x81, 0x81, 0xFF, 0x00};

// Frame buffer — written by web handler, read by loop().
// Protected by frameMutex to prevent mid-write tearing.
uint8_t customFrame[256] = {0};

// --- RoboEyes state ---
volatile int roboMood = 0;
int roboTimer = 0;
int roboBlinkPhase = 0; // 0=No blink, 1=Closing, 2=Closed, 3=Opening
volatile bool manualMood = false;
int roboEyeX = 0, roboEyeY = 0, roboMoveTimer = 0;

// --- Dog state ---
int dogX = 12, dogTargetX = 12, dogState = 0, dogTimer = 0, dogAnimFrame = 0,
    dogDir = 1;
int ballX = -1, ballY = -1, ballVX = 0, ballVY = 0, poopX = -1;

// ==================== 1. CORE DRAWING WRAPPER ====================
// This handles 180 rotation globally
void drawPixel(int r, int c, bool on) {
  if (r < 0 || r >= 8 || c < 0 || c >= 32)
    return;

  int finalR = r;
  int finalC = c;

  // 1. User flips
  if (flipH) finalC = 31 - finalC;
  if (flipV) finalR = 7 - finalR;

  // 2. User rotation
  if (rotation == 180) {
      finalR = 7 - finalR;
      finalC = 31 - finalC;
  }
  // (90 and 270 are omitted for now as they require rectangular mapping)

  // 3. PHYSICAL MOUNTING CORRECTION (Legacy 180)
  // This ensures that with flip=0, rotate=0, it stays same as before.
  int physicalR = 7 - finalR;
  int physicalC = 31 - finalC;

  mx.setPoint(physicalR, physicalC, on ? !isInverse : isInverse);
}

// ==================== 2. ROBO EYES (FluxGarage style) ====================
void drawRoboEye(int startC, int mood, int blinkPhase, bool isLeft) {
  auto draw = [&](int r, int c, bool on) { drawPixel(r, c, on); };

  int height = 5;
  int width = 4;
  int rOffset = 1; // Default row offset
  int cOffset = 1; // Default col offset

  bool shape[8][6] = {false};

  if (mood == 6) { // NAUGHTY
    if (isLeft)
      rOffset = 0;
    else
      rOffset = 2;
  } else if (mood == 5) { // JEALOUS
    if (isLeft) {
      cOffset = 2;
    } else {
      height = 3;
      rOffset = 3;
    }                      // Right eye squinting low
  } else if (mood == 10) { // SICK (squint and shake)
    height = 3;
    rOffset = 2;
    cOffset = 1 + ((millis() / 50) % 3) - 1; // Jitter wildly
  } else if (mood == 12) {                   // ANNOYED (eye roll sequence)
    int phase = (millis() / 150) % 8;
    if (phase == 0) {
      rOffset = 1;
      cOffset = 1;
    } else if (phase == 1) {
      rOffset = 0;
      cOffset = 1;
    } else if (phase == 2) {
      rOffset = 0;
      cOffset = 2;
    } else if (phase == 3) {
      rOffset = 0;
      cOffset = 2;
    } else if (phase == 4) {
      rOffset = 1;
      cOffset = 2;
    } else if (phase >= 5) {
      rOffset = 1;
      cOffset = 1;
    }
  } else if (mood == 11) { // SCARE (jagged wide)
    height = 6;
    width = 5;
    rOffset = 0;
    cOffset = 0;
  } else if (mood == 13) { // SLEEP (closed line)
    height = 1;
    width = 5;
    rOffset = 3;
    cOffset = 0;
  } else if (mood == 14) { // BORED (squint)
    height = 5;
    width = 5;
    rOffset = 1;
    cOffset = 0;
  }

  // Draw base rectangle IF not weather
  if (mood < 7 || mood >= 10) {
    for (int r = 0; r < height; r++) {
      for (int c = 0; c < width; c++) {
        if (r + rOffset < 8 && c + cOffset < 6) {
          shape[r + rOffset][c + cOffset] = true;
        }
      }
    }
  } else {
    // WEATHER ICONS
    if (mood == 7) { // SUN
      shape[1][2] = true;
      shape[2][1] = true;
      shape[2][2] = true;
      shape[2][3] = true;
      shape[3][0] = true;
      shape[3][1] = true;
      shape[3][2] = true;
      shape[3][3] = true;
      shape[3][4] = true;
      shape[4][1] = true;
      shape[4][2] = true;
      shape[4][3] = true;
      shape[5][2] = true;
    } else if (mood == 8) { // RAIN CLOUDS
      shape[2][1] = true;
      shape[2][2] = true;
      shape[2][3] = true;
      shape[3][0] = true;
      shape[3][1] = true;
      shape[3][2] = true;
      shape[3][3] = true;
      shape[3][4] = true;
      if ((millis() / 150) % 2 == 0) {
        shape[5][0] = true;
        shape[6][2] = true;
        shape[5][4] = true;
      } else {
        shape[6][0] = true;
        shape[5][2] = true;
        shape[6][4] = true;
      }
    } else if (mood == 9) { // SNOW
      shape[1][2] = true;
      shape[3][0] = true;
      shape[3][4] = true;
      shape[5][2] = true;
      shape[2][2] = true;
      shape[3][2] = true;
      shape[4][2] = true;
      shape[2][1] = true;
      shape[2][3] = true;
      shape[4][1] = true;
      shape[4][3] = true;
    }
  }

  // Apply mood modifications
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 6; c++) {
      if (!shape[r][c])
        continue;

      if (mood == 1) { // HAPPY
        if (r >= rOffset + 3)
          shape[r][c] = false;
        if (r >= rOffset + 1 && c > cOffset && c < cOffset + width - 1)
          shape[r][c] = false;
      } else if (mood == 2) { // ANGRY
        if (isLeft) {
          if (r == rOffset && c >= cOffset + 2)
            shape[r][c] = false;
          if (r == rOffset + 1 && c == cOffset + 3)
            shape[r][c] = false;
        } else {
          if (r == rOffset && c <= cOffset + 1)
            shape[r][c] = false;
          if (r == rOffset + 1 && c == cOffset)
            shape[r][c] = false;
        }
      } else if (mood == 3) { // SAD/TIRED
        if (isLeft) {
          if (r == rOffset && c <= cOffset + 1)
            shape[r][c] = false;
        } else {
          if (r == rOffset && c >= cOffset + 2)
            shape[r][c] = false;
        }
        if (r >= rOffset + 4)
          shape[r][c] = false;
      } else if (mood == 4) { // SUSPICIOUS
        if (r <= rOffset + 1)
          shape[r][c] = false;
      } else if (mood == 11) { // SCARE
        if (r == 0 && (c == 0 || c == 2 || c == 4))
          shape[r][c] = false;
        if (r == 5 && (c == 1 || c == 3))
          shape[r][c] = false;
        if ((millis() / 50) % 2 == 0 && r == 2 && c == 2)
          shape[r][c] = false;
      } else if (mood == 14) { // BORED (heavy top lid)
        if (r <= rOffset + 1)
          shape[r][c] = false;
        // Add a heavy blink effect occasionally during BORED
        if ((millis() / 2000) % 2 == 0 && r <= rOffset + 2)
          shape[r][c] = false;
      }
    }
  }

  // Apply blink phase (Squint / Fully Closed) - Skip blinks for weather
  if (mood < 7 || mood >= 10) {
    if (blinkPhase == 1 || blinkPhase == 3) { // Squint
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 6; c++) {
          if (r <= rOffset || r >= rOffset + height - 1)
            shape[r][c] = false;
        }
      }
    } else if (blinkPhase == 2) { // Fully closed
      for (int r = 0; r < 8; r++) {
        for (int c = 0; c < 6; c++) {
          if (r != rOffset + height / 2)
            shape[r][c] = false;
        }
      }
    }
  }

  // Draw final shape
  int finalX = roboEyeX;
  int finalY = roboEyeY;

  // SLEEP logic: 30s Eyes / 30s ZZZ text
  if (mood == 13) {
    blinkPhase = 2; // Keep closed

    // Cycle: 0-30s = Eyes, 30-60s = ZZZZ
    bool showText = ((millis() / 30000) % 2 == 1);

    if (showText) {
      // Clear eyes so we only see text
      for (int r = 0; r < 8; r++)
        for (int c = 0; c < 6; c++)
          shape[r][c] = false;

      // Draw scrolling "zzzzZZZZ" only once (for left eye call)
      if (isLeft) {
        int scrollX = 32 - ((millis() / 150) % 64); // Scroll right to left
        // Draw small 'z'
        auto drawZ = [&](int x, int y, bool big) {
          // FIXED: Use correct inversion for the Z characters (user said they
          // were inverted)
          if (big) { // 5x5 Z
            for (int i = 0; i < 5; i++) {
              draw(y, x + i, true);
              draw(y + 4, x + i, true);
              draw(y + 4 - i, x + i, true);
            }
          } else { // 3x3 z
            for (int i = 0; i < 3; i++) {
              draw(y, x + i, true);
              draw(y + 2, x + i, true);
              draw(y + 2 - i, x + i, true);
            }
          }
        };
        drawZ(scrollX, 4, false);
        drawZ(scrollX + 6, 3, false);
        drawZ(scrollX + 12, 4, false);
        drawZ(scrollX + 20, 2, true);
        drawZ(scrollX + 28, 2, true);
        drawZ(scrollX + 38, 1, true);
      }
    } else {
      // Smooth breathing eyes (Sine wave)
      finalY += (int)(sin(millis() / 1500.0) * 1.5 + 0.5);
    }
  }

  // NAUGHTY logic: Shrink and grow smoothly
  if (mood == 6) {
    float scale = sin(millis() / 800.0) * 0.2 + 0.9; // Scale 0.7 to 1.1
    // Apply center-out Scaling by simply adjusting height/offset proportionally
    // (simplified)
    if (scale < 0.9)
      finalY += 1;
  }

  // BORED logic: smooth organic scanning
  if (mood == 14) {
    finalX = (int)(sin(millis() / 3000.0) * 5.0 +
                   cos(millis() / 1500.0) * 2.0); // Wavy scanning
  }

  // DANCE logic: Rhythmic bounce
  if (mood == 17) {
    finalX += (int)(sin(millis() / 400.0) * 4.0);
    finalY += (int)(fabs(cos(millis() / 400.0)) * -3.0);
  }

  // SING logic: Floating musical notes (hide eyes)
  if (mood == 18) {
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 6; c++)
        shape[r][c] = false;
    if (isLeft) {
      for (int i = 0; i < 2; i++) {
        int noteX = 28 - ((millis() / (250 + i * 50)) % 24);
        int noteY = 2 + (int)(sin((millis() + i * 1000) / 400.0) * 2.0);
        if (i == 0) { // Single note ♪
          draw(noteY, noteX, true);
          draw(noteY + 1, noteX, true);
          draw(noteY + 2, noteX, true);
          draw(noteY, noteX + 1, true);
        } else { // Double note ♫
          draw(noteY, noteX, true);
          draw(noteY, noteX + 2, true);
          draw(noteY + 1, noteX, true);
          draw(noteY + 1, noteX + 2, true);
          draw(noteY, noteX + 1, true);
        }
      }
    }
  }

  // STARS logic: Twinkling background (hide eyes)
  if (mood == 16) {
    for (int r = 0; r < 8; r++)
      for (int c = 0; c < 6; c++)
        shape[r][c] = false;
    if (isLeft) {
      for (int i = 0; i < 6; i++) {
        int sx = (i * 137 + millis() / 100) % 32;
        int sy = (i * 257 + millis() / 150) % 8;
        if ((millis() / (200 + i * 50)) % 2 == 0)
          draw(sy, sx, true);
      }
    }
  }

  // ==================== EXTRA EXPRESSIONS ====================

  // DIZZY (19) — spinning spiral in each eye
  if (mood == 19) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    float ang = millis() / 400.0;
    for (int i = 0; i < 8; i++) {
      float a = ang + i * 0.8;
      float radius = 0.3 + i * 0.35;
      int sr = (int)(3.0 + sin(a) * radius + 0.5);
      int sc = (int)(2.5 + cos(a) * radius + 0.5);
      if (sr >= 0 && sr < 8 && sc >= 0 && sc < 6) shape[sr][sc] = true;
    }
  }

  // LOVE (20) — pulsing heart eyes
  if (mood == 20) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    float pulse = 1.0 + sin(millis() / 400.0) * 0.25;
    const int heartPts[][2] = {{0,1},{0,3},{1,0},{1,1},{1,2},{1,3},{1,4},{2,0},{2,1},{2,2},{2,3},{2,4},{3,1},{3,2},{3,3},{4,2}};
    for (int i = 0; i < 16; i++) {
      int pr = (int)((heartPts[i][0] - 2) * pulse + 2 + 0.5);
      int pc = (int)((heartPts[i][1] - 2) * pulse + 2 + 0.5);
      if (pr >= 0 && pr < 8 && pc >= 0 && pc < 6) shape[pr][pc] = true;
    }
  }

  // SAD_CRY (21) — sad eyes + falling tears
  if (mood == 21) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    for (int r = 0; r < 4; r++) for (int c = 0; c < width; c++)
      if (r + rOffset < 8 && c + cOffset < 6) shape[r + rOffset][c + cOffset] = true;
    if (isLeft) { shape[rOffset][cOffset] = false; shape[rOffset][cOffset + 1] = false; }
    else { shape[rOffset][cOffset + 2] = false; shape[rOffset][cOffset + 3] = false; }
    int td = (millis() / 200) % 4;
    draw(5 + td, startC + 2 + finalX, true);
    if (td > 0) draw(4 + td, startC + 3 + finalX, true);
  }

  // THINK (22) — normal eyes + thought bubble dots growing
  if (mood == 22 && !isLeft) {
    int dots = (millis() / 800) % 4;
    if (dots >= 1) draw(0, startC + 5 + finalX, true);
    if (dots >= 2) draw(0, startC + 3 + finalX, true);
    if (dots >= 3) { draw(0, startC + 1 + finalX, true); draw(0, startC + 4 + finalX, true); }
    if (dots >= 2) { draw(1, startC + 4 + finalX, true); draw(1, startC + 5 + finalX, true); }
  }

  // SNEEZE (23) — jitter then burst
  if (mood == 23) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    if ((millis() % 2000) < 1500) {
      int sh = ((millis() / 50) % 2 == 0) ? 1 : -1;
      for (int r = 0; r < 3; r++) for (int c = 0; c < 4; c++) {
        int nc = 2 + c + sh;
        if (nc >= 0 && nc < 6) shape[r + 3][nc] = true;
      }
    } else {
      for (int i = 0; i < 8; i++) shape[random(0, 8)][random(0, 6)] = true;
    }
  }

  // WINK (24) — right eye closed line
  if (mood == 24 && !isLeft) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    for (int c = 0; c < 4; c++) shape[4][c + 1] = true;
  }

  // HAPPY_CRY (25) — happy arch eyes + tears of joy
  if (mood == 25) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    shape[rOffset][cOffset] = true; shape[rOffset][cOffset + width - 1] = true;
    shape[rOffset + 1][cOffset] = true; shape[rOffset + 1][cOffset + width - 1] = true;
    shape[rOffset + 2][cOffset] = true; shape[rOffset + 2][cOffset + width - 1] = true;
    for (int c = 0; c < width; c++) shape[rOffset][cOffset + c] = true;
    int ht = (millis() / 250) % 4;
    draw(rOffset + 3 + ht, startC + 1 + finalX, true);
    draw(rOffset + 3 + ((ht + 2) % 4), startC + 4 + finalX, true);
  }

  // EXCLAIM (26) — flashing ! marks
  if (mood == 26) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    for (int r = 0; r < 4; r++) { shape[r][2] = true; shape[r][3] = true; }
    shape[5][2] = true; shape[5][3] = true;
    if ((millis() / 300) % 2 == 0) { shape[6][2] = true; shape[6][3] = true; }
  }

  // QUESTION (27) — wobbling ? marks
  if (mood == 27) {
    for (int r = 0; r < 8; r++) for (int c = 0; c < 6; c++) shape[r][c] = false;
    shape[0][1] = true; shape[0][2] = true; shape[0][3] = true;
    shape[1][3] = true; shape[1][4] = true;
    shape[2][3] = true; shape[3][2] = true; shape[3][3] = true;
    shape[4][2] = true;
    shape[6][2] = true;
    finalX += (int)(sin(millis() / 500.0) * 1.0);
  }

  // ==================== DRAW FINAL SHAPE ====================
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 6; c++) {
      if (shape[r][c])
        draw(r + finalY, startC + c + finalX, true);
    }
  }
}

void animRoboEyes() {
  if (roboBlinkPhase == 0) {
    if (random(100) < 5)
      roboBlinkPhase = 1;
  } else {
    roboBlinkPhase++;
    if (roboBlinkPhase > 3)
      roboBlinkPhase = 0;
  }

  if (!manualMood) {
    if (roboTimer <= 0) {
      // Pick random mood, but skip weather (7-9)
      int rMood = random(0, 10); // 0-9 range
      if (rMood >= 7)
        rMood += 3; // Skip 7,8,9 → becomes 10,11,12
      roboMood = rMood;
      roboTimer = random(30, 100);
    } else {
      roboTimer--;
    }
  }

  if (roboMoveTimer <= 0) {
    if (roboMood < 7 || roboMood >= 10) {
      if (random(100) < 60) {
        roboEyeX = 0;
        roboEyeY = 0;
        roboMoveTimer = random(20, 50);
      } else {
        roboEyeX = random(-2, 3);
        roboEyeY = random(-1, 2);
        roboMoveTimer = random(10, 30);
      }
    } else {
      roboEyeX = 0;
      roboEyeY = 0;
      roboMoveTimer = 10;
    }
  } else {
    roboMoveTimer--;
  }

  drawRoboEye(6, roboMood, roboBlinkPhase, true);   // Left eye
  drawRoboEye(20, roboMood, roboBlinkPhase, false); // Right eye
}

// ==================== 12. DOG REDESIGN (mode a) ====================
void drawDog(int x, int state, int frame, int dir) {
  auto draw = [&](int r, int relC, bool on) {
    int drawC = x + (relC * dir);
    drawPixel(r, drawC, on);
  };

  // Head
  draw(3, 5, true);
  draw(3, 6, true);
  draw(3, 7, true);
  draw(4, 5, true);
  draw(4, 6, true);
  draw(4, 7, true);
  draw(4, 8, true);
  draw(2, 6, true);
  draw(3, 8, false);
  if (state == 0 && frame % 2 == 0)
    draw(5, 8, true);

  // Floppy ear
  if (frame % 2 == 0) {
    draw(3, 4, true);
    draw(4, 4, true);
  } else {
    draw(4, 4, true);
    draw(5, 4, true);
  }

  // Body
  for (int r = 5; r <= 6; r++)
    for (int c = 1; c <= 6; c++)
      draw(r, c, true);

  // Tail
  if (state == 0 || state == 2 || state == 4) {
    if (frame % 2 == 0) {
      draw(3, 0, true);
      draw(4, 0, true);
    } else {
      draw(4, 0, true);
      draw(5, -1, true);
    }
  } else {
    draw(4, 0, true);
  }

  // Legs & Context
  if (state == 0) {
    draw(6, 1, false);
    draw(7, 2, true);
    draw(7, 5, true);
    draw(7, 6, true);
  } else if (state == 1) {
    if (frame % 2 == 0) {
      draw(7, 2, true);
      draw(7, 5, true);
    } else {
      draw(7, 1, true);
      draw(7, 6, true);
    }
  } else if (state == 4) {
    draw(6, 1, false);
    draw(7, 2, true);
    draw(7, 5, true);
    if (frame % 4 < 2)
      draw(1, 9, true);
    else
      draw(0, 10, true);
  } else if (state == 5) {
    draw(7, 1, true);
    draw(7, 3, true);
    if (frame % 2 == 0) {
      draw(6, 9, true);
      draw(7, 10, true);
    } else {
      draw(5, 10, true);
      draw(7, 9, true);
    }
  } else {
    draw(7, 2, true);
    draw(7, 5, true);
  }
}

void animDog() {
  dogAnimFrame++;
  if (dogTimer <= 0) {
    if (dogState == 3 && poopX != -1) {
      dogState = 1;
      dogTargetX = constrain(poopX + 12, 0, 20);
      dogTimer = 30;
    } else {
      int next = random(0, 100);
      if (next < 30) {
        dogState = 1;
        dogTargetX = random(0, 20);
        dogTimer = random(30, 80);
      } else if (next < 50) {
        dogState = 2;
        dogTimer = random(50, 120);
        ballX = dogX + (dogDir * 12);
        if (ballX > 28 || ballX < 0)
          ballX = dogX - (dogDir * 8);
        ballY = 2;
        ballVX = (ballX > dogX) ? -1 : 1;
        ballVY = 1;
      } else if (next < 60) {
        dogState = 3;
        dogTimer = 25;
        poopX = -1;
      } else if (next < 75) {
        dogState = 4;
        dogTimer = random(40, 100);
      } else if (next < 85) {
        dogState = 5;
        dogTimer = random(30, 60);
      } else {
        dogState = 0;
        dogTimer = random(30, 60);
      }
    }
  }
  dogTimer--;

  if (dogState == 1) {
    if (dogX < dogTargetX) {
      dogX++;
      dogDir = 1;
    } else if (dogX > dogTargetX) {
      dogX--;
      dogDir = -1;
    } else
      dogState = 0;
  }
  if (dogState == 3 && dogTimer == 10)
    poopX = dogX - (dogDir * 2);
  if (dogState == 2) {
    if (ballX > dogX + 5)
      dogDir = 1;
    else if (ballX < dogX - 5)
      dogDir = -1;
    ballX += ballVX;
    ballY += ballVY;
    if (ballY >= 6) {
      ballY = 6;
      ballVY = -1;
    }
    if (ballY <= 1) {
      ballY = 1;
      ballVY = 1;
    }
    if (ballX <= 0 || ballX >= 30)
      ballVX *= -1;
    int mouthX = dogX + (dogDir * 9);
    if (mouthX < ballX)
      dogX++;
    else if (mouthX > ballX)
      dogX--;
    if (abs(ballX - mouthX) <= 2 && ballY >= 3) {
      ballVX = (ballX > dogX) ? 1 : -1;
      ballVY = -1;
    }
  } else {
    ballX = -1;
  }

  if (poopX >= 0 && poopX < 32) {
    drawPixel(7, poopX, true);
    drawPixel(7, poopX + 1, true);
    drawPixel(6, poopX + 1, true);
  }
  if (ballX >= 0 && ballX < 31 && ballY >= 0 && ballY < 7) {
    drawPixel(ballY, ballX, true);
    drawPixel(ballY + 1, ballX, true);
    drawPixel(ballY, ballX + 1, true);
    drawPixel(ballY + 1, ballX + 1, true);
  }
  drawDog(dogX, dogState, dogAnimFrame, dogDir);
}

// ==================== NETWORK EVENT HANDLER ====================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client,
               AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo *)arg;
    if (info->final && info->index == 0 && info->len == len &&
        info->opcode == WS_TEXT) {
      data[len] = 0;
      String msg = (char *)data;
      if (msg.startsWith("cmd:mode:")) {
        String modeStr = msg.substring(9);
        if (modeStr == "2")
          currentMode = 2;
        else if (modeStr == "a")
          currentMode = 12;
        else if (modeStr == "99")
          currentMode = 99;
      } else if (msg.startsWith("cmd:mood:")) {
        String moodStr = msg.substring(9);
        if (moodStr == "auto")
          manualMood = false;
        else {
          roboMood = moodStr.toInt();
          manualMood = true;
          currentMode = 2;
        }
      } else {
        networkText = msg;
        currentMode = 99;
      }
    }
  }
}

// --- Game state (Bounce) ---
float ballQX = 16.0, ballQY = 4.0, ballQVX = 1.1, ballQVY = 0.7;

void drawBounceGame() {
  // Update ball position
  ballQX += ballQVX;
  ballQY += ballQVY;

  // Bounce across full screen (0 to 31)
  if (ballQX <= 0.0) {
    ballQX = 0.0;
    ballQVX = std::abs(ballQVX);
  }
  if (ballQX >= 31.0) {
    ballQX = 31.0;
    ballQVX = -std::abs(ballQVX);
  }
  if (ballQY <= 0.0) {
    ballQY = 0.0;
    ballQVY = std::abs(ballQVY);
  }
  if (ballQY >= 7.0) {
    ballQY = 7.0;
    ballQVY = -std::abs(ballQVY);
  }

  // Draw ball and a short trail (no eye — full screen game)
  int bx = (int)ballQX;
  int by = (int)ballQY;
  drawPixel(by, bx, true);

  // Trail (previous position approximation)
  int tx = (int)(ballQX - ballQVX);
  int ty = (int)(ballQY - ballQVY);
  if (tx >= 0 && tx <= 31 && ty >= 0 && ty <= 7) {
    drawPixel(ty, tx, true);
  }
}

// ==================== STATS & ICONS ====================
void drawChar(int startC, char c) {
  for (int r = 0; r < 7; r++) {
    for (int b = 0; b < 5; b++) {
      if (startC + b >= 0 && startC + b < 32) {
        bool pixel = false;
        if (c >= '0' && c <= '9') {
          int digit = c - '0';
          static const uint8_t font[10][5] = {
              {0x3E, 0x51, 0x49, 0x45, 0x3E}, // 0
              {0x00, 0x42, 0x7F, 0x40, 0x00}, // 1
              {0x42, 0x61, 0x51, 0x49, 0x46}, // 2
              {0x21, 0x41, 0x45, 0x4B, 0x31}, // 3
              {0x18, 0x14, 0x12, 0x7F, 0x10}, // 4
              {0x27, 0x45, 0x45, 0x45, 0x39}, // 5
              {0x3C, 0x4A, 0x49, 0x49, 0x30}, // 6
              {0x01, 0x71, 0x09, 0x05, 0x03}, // 7
              {0x36, 0x49, 0x49, 0x49, 0x36}, // 8
              {0x06, 0x49, 0x49, 0x29, 0x1E}  // 9
          };
          pixel = (font[digit][b] & (1 << r)) != 0;
        } else if (c == ':') {
          pixel = (r == 2 || r == 4) && b == 2;
        } else if (c == '%') {
          pixel = ((r == 0 || r == 6) && b < 4) ||
                  ((r == 1 || r == 5) && (b == 0 || b == 3)) ||
                  ((r == 2 || r == 4) && (b == 1 || b == 2)) ||
                  (r == 3 && b == 2);
        } else if (c == 'C' || c == 'c') {
          pixel = b == 0 || r == 0 || r == 6;
        } else if (c == 'G' || c == 'g') {
          pixel = b == 0 || r == 0 || r == 6 || (r >= 3 && b == 4) || (r == 3 && b >= 2);
        } else if (c == '.') {
          pixel = (r == 6 && b == 2);
        }
        if (pixel) drawPixel(r, startC + b, true);
      }
    }
  }
}

void drawIcon(int startC, const uint8_t *bitmap) {
  for (int c = 0; c < 8; c++) {
    for (int r = 0; r < 8; r++) {
      if (bitmap[c] & (1 << r)) {
        drawPixel(r, startC + c, true);
      }
    }
  }
}

void drawStatCard(const uint8_t *icon, String valStr) {
  drawIcon(0, icon);
  int textWidth = valStr.length() * 6 - 1;
  
  // If text is too wide, don't center, just start at 9
  int startC = (textWidth > 23) ? 9 : 8 + (24 - textWidth) / 2;
  
  for (int i = 0; i < valStr.length(); i++) {
    drawChar(startC + i * 6, valStr[i]);
  }
}

void saveConfig() {
    prefs.begin("led_matrix", false);
    prefs.putBool("flipH", flipH);
    prefs.putBool("flipV", flipV);
    prefs.putInt("rotation", rotation);
    prefs.putInt("intensity", intensity);
    prefs.putBool("isInverse", isInverse);
    prefs.end();
}

void loadConfig() {
    prefs.begin("led_matrix", true);
    flipH = prefs.getBool("flipH", false);
    flipV = prefs.getBool("flipV", false);
    rotation = prefs.getInt("rotation", 0);
    intensity = prefs.getInt("intensity", 0);
    isInverse = prefs.getBool("isInverse", true);
    prefs.end();
}

void animNetwork() {
  // Display static centered text
  if (networkText.length() == 0) {
    // Fallback to simple indicator
    if ((millis() / 500) % 2 == 0) {
      mx.setPoint(0, 0, !isInverse);
      mx.setPoint(0, 31, !isInverse);
    }
    return;
  }

  // Text width: 6 pixels per char (5 + 1 gap).
  String displayText = networkText;
  int textWidth = displayText.length() * 6;
  
  if (textWidth <= 32) {
    // Center if fits
    int startC = (32 - textWidth + 1) / 2;
    for (int i = 0; i < displayText.length(); i++) {
        drawChar(startC + i * 6, displayText[i]);
    }
  } else {
    // Scroll if too long
    int scrollPos = 32 - ((millis() / 50) % (textWidth + 32));
    for (int i = 0; i < displayText.length(); i++) {
        drawChar(scrollPos + i * 6, displayText[i]);
    }
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  loadConfig();
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();

  Serial.print("WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long wifiStart = millis();
  int dot = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    drawPixel(4, dot % 32, true);
    dot++;
    delay(200);
    Serial.print(".");
  }

  if (WiFi.status() == WL_CONNECTED) {
    wifiConnected = true;
    configTime(UTC_OFFSET, 0, "pool.ntp.org", "time.nist.gov");
    Serial.println(" OK!");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
    time_t now = time(nullptr);
    unsigned long ntpStart = millis();
    while (now < 100000 && millis() - ntpStart < 5000) {
      delay(300);
      now = time(nullptr);
    }
    Serial.println("NTP synced!");

    // Setup API
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);

    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
      request->send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("c")) {
        String cStr = request->getParam("c")->value();
        if (cStr.length() > 0) {
          char cmd = cStr.charAt(0);
          if (cmd == 'i' || cmd == 'I')
            isInverse = !isInverse;
          if (cmd == '+' || cmd == '=') {
            if (intensity < 15)
              intensity++;
            mx.control(MD_MAX72XX::INTENSITY, intensity);
          }
          if (cmd == '-' || cmd == '_') {
            if (intensity > 0)
              intensity--;
            mx.control(MD_MAX72XX::INTENSITY, intensity);
          }
        }
      }
      request->send(200, "text/plain", "Command executed");
    });

    server.on("/mood", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("set")) {
        String moodStr = request->getParam("set")->value();
        if (moodStr == "auto")
          manualMood = false;
        int mVal = moodStr.toInt();
        if (mVal == 15) {
          currentMode = 15;
        } else {
          roboMood = mVal;
          manualMood = true;
          currentMode = 2;
        }
      }
      request->send(200, "text/plain", "Mood updated");
    });

    server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("set")) {
        String modeStr = request->getParam("set")->value();
        if (modeStr == "2")
          currentMode = 2;
        else if (modeStr == "12")
          currentMode = 12;
        else if (modeStr == "15")
          currentMode = 15;
        else if (modeStr == "a")
          currentMode = 12;
        else if (modeStr == "99")
          currentMode = 99;
      }
      request->send(200, "text/plain", "Mode updated");
    });

    server.on("/text", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("msg")) {
        networkText = request->getParam("msg")->value();
        currentMode = 99;
      }
      request->send(200, "text/plain", "Text updated");
    });

    server.on("/intensity", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("set")) {
        intensity = request->getParam("set")->value().toInt();
        intensity = constrain(intensity, 0, 15);
        mx.control(MD_MAX72XX::INTENSITY, intensity);
        saveConfig();
      }
      request->send(200, "text/plain", "Intensity updated");
    });

    server.on("/config", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("flipH")) flipH = request->getParam("flipH")->value() == "1";
      if (request->hasParam("flipV")) flipV = request->getParam("flipV")->value() == "1";
      if (request->hasParam("rotate")) rotation = request->getParam("rotate")->value().toInt();
      saveConfig();
      request->send(200, "text/plain", "Config updated");
    });

    server.on("/stats", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("cpu")) cpuUsage = request->getParam("cpu")->value();
      if (request->hasParam("ram")) ramUsage = request->getParam("ram")->value();
      if (request->hasParam("temp")) tempValue = request->getParam("temp")->value();
      if (request->hasParam("batt")) battUsage = request->getParam("batt")->value();
      currentMode = 100; // Stats mode
      request->send(200, "text/plain", "Stats updated");
    });

    server.on("/shout", HTTP_GET, [](AsyncWebServerRequest *request) {
      if (request->hasParam("msg")) {
        shoutMsg = request->getParam("msg")->value();
        shoutSpeed = request->hasParam("speed") ? request->getParam("speed")->value().toInt() : 50;
        int pause = request->hasParam("pause") ? request->getParam("pause")->value().toInt() : 3000;
        shoutEndTime = millis() + pause;
        if (currentMode != 101) shoutModeOld = currentMode;
        currentMode = 101; // Shout mode
      }
      request->send(200, "text/plain", "Shout triggered");
    });

    server.begin();
    Serial.println("Web Server running on port 80");
  } else {
    Serial.println(" SKIP (no WiFi — modes 7,8 limited)");
  }

  // --- Boot Sequence: Yawn ---
  mx.clear();
  for (int h = 0; h <= 4; h++) {
    mx.clear();
    // Expand height gradually
    for (int r = 4 - h / 2; r <= 4 + h / 2; r++) {
      for (int c = 6; c < 11; c++)
        drawPixel(r, c, true);
      for (int c = 20; c < 25; c++)
        drawPixel(r, c, true);
    }
    mx.update();
    delay(150);
  }
  mx.clear();

  Serial.println("--- LED MATRIX CONTROLLER ---");
  Serial.println("2:RoboEyes | a:Dog");
  Serial.println("i:Inverse | +/-:Brightness");
}

// ==================== MAIN LOOP ====================
void loop() {
  if (wifiConnected) {
    ws.cleanupClients();
  }

  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '2')
      currentMode = 2;
    if (cmd == 'a' || cmd == 'A') {
      currentMode = 12;
      dogState = 0;
      ballX = -1;
      poopX = -1;
    }
    if (cmd == 'i' || cmd == 'I')
      isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') {
      if (intensity < 15)
        intensity++;
      mx.control(MD_MAX72XX::INTENSITY, intensity);
    }
    if (cmd == '-' || cmd == '_') {
      if (intensity > 0)
        intensity--;
      mx.control(MD_MAX72XX::INTENSITY, intensity);
    }
    mx.clear();
  }

  if (millis() - lastTick < (unsigned long)animDelay)
    return;
  lastTick = millis();
  flip = !flip;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++)
    for (int c = 0; c < 32; c++)
      mx.setPoint(r, c, isInverse);

  switch (currentMode) {
  case 2:
    animRoboEyes();
    break;
  case 12:
    animDog();
    break;
  case 15:
    drawBounceGame();
    break;
  case 99:
    animNetwork();
    break;
  case 100: // Stats mode
    if (millis() - lastStatCycle > 4000) {
        lastStatCycle = millis();
        currentStatIndex = (currentStatIndex + 1) % 4;
    }
    if (currentStatIndex == 0) drawStatCard(ICON_CPU, cpuUsage);
    else if (currentStatIndex == 1) drawStatCard(ICON_RAM, ramUsage);
    else if (currentStatIndex == 2) drawStatCard(ICON_TEMP, tempValue);
    else drawStatCard(ICON_BATT, battUsage);
    break;
  case 101: // Shout mode
    {
        int textWidth = shoutMsg.length() * 6;
        int scrollPos = 32 - ((millis() / shoutSpeed) % (textWidth + 32));
        for (int i = 0; i < shoutMsg.length(); i++) {
            drawChar(scrollPos + i * 6, shoutMsg[i]);
        }
        if (millis() > shoutEndTime) {
            currentMode = shoutModeOld;
        }
    }
    break;
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}