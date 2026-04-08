#include <Arduino.h>
#include <MD_MAX72xx.h>
#include <SPI.h>
#include <WiFi.h> // Changed for ESP32
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <time.h>
#include <stdint.h>
#include <ESPAsyncWebServer.h>
#include <AsyncTCP.h>

// ==================== HARDWARE CONFIG ====================
#define HARDWARE_TYPE MD_MAX72XX::FC16_HW
#define MAX_DEVICES 4

// Custom one-sided ESP32 pins
#define DATA_PIN 13  // D13 on the board
#define CLK_PIN  14  // D14 on the board
#define CS_PIN   27  // D27 on the board

// Custom software SPI constructor
MD_MAX72XX mx = MD_MAX72XX(HARDWARE_TYPE, DATA_PIN, CLK_PIN, CS_PIN, MAX_DEVICES);

// ==================== WiFi CONFIG ====================
const char* WIFI_SSID = "OhhMarr";
const char* WIFI_PASS = "omaromar";
const long  UTC_OFFSET = 19800;  // IST = UTC+5:30
bool wifiConnected = false;

AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
String networkText = "";
int scrollPos = 0;

const char INDEX_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>LED Matrix Control Panel</title>
  <style>
    body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; text-align: center; background-color: #1a1a2e; color: #fff; margin: 0; padding: 20px; }
    h1 { color: #4ecca3; margin-bottom: 30px; }
    h3 { color: #eeeeee; border-bottom: 1px solid #333; padding-bottom: 10px; max-width: 600px; margin: 20px auto; }
    .btn { display: inline-block; padding: 12px 20px; font-size: 16px; font-weight: bold; cursor: pointer; 
           text-align: center; outline: none; border: none; border-radius: 8px; margin: 8px; width: 140px; 
           color: #1a1a2e; background-color: #4ecca3; box-shadow: 0 4px #3b9b7c; transition: all 0.1s ease; }
    .btn:hover { background-color: #45b793; }
    .btn:active { background-color: #3b9b7c; box-shadow: 0 1px #2a6f59; transform: translateY(3px); }
    .sys-btn { background-color: #e94560; box-shadow: 0 4px #b8364a; color: white; }
    .sys-btn:hover { background-color: #d13d56; }
    .sys-btn:active { background-color: #b8364a; box-shadow: 0 1px #8f2a3a; transform: translateY(3px); }
    .container { max-width: 800px; margin: 0 auto; }
  </style>
  <script>
    function setMode(mode) { fetch('/mode?set=' + mode); }
    function setMood(mood) { fetch('/mood?set=' + mood); }
    function sendCmd(cmd) { fetch('/cmd?c=' + encodeURIComponent(cmd)); }
  </script>
</head>
<body>
  <div class="container">
    <h1>LED Matrix Control Panel</h1>
    
    <h3>Animations</h3>
    <button class="btn" onclick="setMode('2')">Robo Eyes</button>
    <button class="btn" onclick="setMode('a')">Playful Dog</button>
    <button class="btn" onclick="setMode('15')">Start Game</button>
    <button class="btn" style="background-color:#f9d949; color:#1a1a2e;" onclick="setMood(16)">Stars</button>
    <button class="btn" style="background-color:#ff4d4d; color:#fff;" onclick="setMood(17)">Dance</button>
    <button class="btn" style="background-color:#4ecca3; color:#1a1a2e;" onclick="setMood(18)">Sing</button>
    <button class="btn" style="background-color:#533483; color:#fff; box-shadow:0 4px #3d2661;" onclick="setMode('99')">Network Mode</button>
    
    <br><br>
    <h3>Display Settings</h3>
    <button class="btn sys-btn" onclick="sendCmd('+')">Brightness +</button>
    <button class="btn sys-btn" onclick="sendCmd('-')">Brightness -</button>
    <button class="btn sys-btn" onclick="sendCmd('i')">Invert LEDs</button>
  </div>
</body>
</html>
)rawliteral";

// ==================== GLOBAL STATE ====================
// NOTE: These are written by AsyncWebServer (FreeRTOS task) and read by loop().
// `volatile` prevents the compiler from caching them in registers across task switches.
volatile int  currentMode = 2; // Default to Robo Eyes
volatile bool isInverse   = true;
volatile int  intensity   = 0; // Lowest brightness
unsigned long lastTick = 0;
int  animDelay   = 80;
bool flip        = false;

// Frame buffer — written by web handler, read by loop().
// Protected by frameMutex to prevent mid-write tearing.
uint8_t customFrame[256] = {0};

// --- System Stats ---
String currentTime = "";
String batteryStatus = "";
String cpuStatus = "";
String gpuStatus = "";
String ramStatus = "";
String uptimeStatus = "";
String internetStatus = "";
unsigned long lastStatsFetch = 0;
int statsIndex = 0;
String statsList[7];

// --- RoboEyes state ---
volatile int  roboMood      = 0;
int  roboTimer = 0;
int  roboBlinkPhase = 0; // 0=No blink, 1=Closing, 2=Closed, 3=Opening
volatile bool manualMood    = false;
int roboEyeX = 0, roboEyeY = 0, roboMoveTimer = 0;

// --- Dog state ---
int dogX = 12, dogTargetX = 12, dogState = 0, dogTimer = 0, dogAnimFrame = 0, dogDir = 1;
int ballX = -1, ballY = -1, ballVX = 0, ballVY = 0, poopX = -1;

// ==================== 2. ROBO EYES (FluxGarage style) ====================
void drawRoboEye(int startC, int mood, int blinkPhase, bool isLeft) {
  auto draw = [&](int r, int c, bool on) {
    if (r >= 0 && r < 8 && c >= 0 && c < 32) mx.setPoint(r, c, on ? !isInverse : isInverse);
  };
  
  int height = 5;
  int width = 4;
  int rOffset = 1; // Default row offset
  int cOffset = 1; // Default col offset

  bool shape[8][6] = {false};
  
  if (mood == 6) { // NAUGHTY
    if (isLeft) rOffset = 0; else rOffset = 2;
  } else if (mood == 5) { // JEALOUS
    if (isLeft) { cOffset = 2; }
    else { height = 3; rOffset = 3; } // Right eye squinting low
  } else if (mood == 10) { // SICK (squint and shake)
    height = 3; rOffset = 2; 
    cOffset = 1 + ((millis() / 50) % 3) - 1; // Jitter wildly
  } else if (mood == 12) { // ANNOYED (eye roll sequence)
    int phase = (millis() / 150) % 8;
    if (phase == 0) { rOffset = 1; cOffset = 1; }
    else if (phase == 1) { rOffset = 0; cOffset = 1; }
    else if (phase == 2) { rOffset = 0; cOffset = 2; }
    else if (phase == 3) { rOffset = 0; cOffset = 2; }
    else if (phase == 4) { rOffset = 1; cOffset = 2; }
    else if (phase >= 5) { rOffset = 1; cOffset = 1; }
  } else if (mood == 11) { // SCARE (jagged wide)
    height = 6; width = 5; rOffset = 0; cOffset = 0;
  } else if (mood == 13) { // SLEEP (closed line)
    height = 1; width = 5; rOffset = 3; cOffset = 0;
  } else if (mood == 14) { // BORED (squint)
    height = 5; width = 5; rOffset = 1; cOffset = 0;
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
      shape[1][2]=true;
      shape[2][1]=true; shape[2][2]=true; shape[2][3]=true;
      shape[3][0]=true; shape[3][1]=true; shape[3][2]=true; shape[3][3]=true; shape[3][4]=true;
      shape[4][1]=true; shape[4][2]=true; shape[4][3]=true;
      shape[5][2]=true;
    } else if (mood == 8) { // RAIN CLOUDS
      shape[2][1]=true; shape[2][2]=true; shape[2][3]=true;
      shape[3][0]=true; shape[3][1]=true; shape[3][2]=true; shape[3][3]=true; shape[3][4]=true;
      if ((millis() / 150) % 2 == 0) { shape[5][0]=true; shape[6][2]=true; shape[5][4]=true; }
      else                           { shape[6][0]=true; shape[5][2]=true; shape[6][4]=true; }
    } else if (mood == 9) { // SNOW
      shape[1][2]=true; shape[3][0]=true; shape[3][4]=true; shape[5][2]=true; 
      shape[2][2]=true; shape[3][2]=true; shape[4][2]=true;
      shape[2][1]=true; shape[2][3]=true; shape[4][1]=true; shape[4][3]=true;
    }
  }

  // Apply mood modifications
  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 6; c++) {
       if (!shape[r][c]) continue;
       
       if (mood == 1) { // HAPPY
         if (r >= rOffset + 3) shape[r][c] = false;
         if (r >= rOffset + 1 && c > cOffset && c < cOffset + width - 1) shape[r][c] = false; 
       } else if (mood == 2) { // ANGRY
         if (isLeft) {
           if (r == rOffset && c >= cOffset + 2) shape[r][c] = false;
           if (r == rOffset + 1 && c == cOffset + 3) shape[r][c] = false;
         } else {
           if (r == rOffset && c <= cOffset + 1) shape[r][c] = false;
           if (r == rOffset + 1 && c == cOffset) shape[r][c] = false;
         }
       } else if (mood == 3) { // SAD/TIRED
         if (isLeft) {
           if (r == rOffset && c <= cOffset + 1) shape[r][c] = false;
         } else {
           if (r == rOffset && c >= cOffset + 2) shape[r][c] = false;
         }
         if (r >= rOffset + 4) shape[r][c] = false;
       } else if (mood == 4) { // SUSPICIOUS
         if (r <= rOffset + 1) shape[r][c] = false;
       } else if (mood == 11) { // SCARE
         if (r == 0 && (c == 0 || c == 2 || c == 4)) shape[r][c] = false;
         if (r == 5 && (c == 1 || c == 3)) shape[r][c] = false;
         if ((millis()/50)%2 == 0 && r == 2 && c == 2) shape[r][c] = false; 
       } else if (mood == 14) { // BORED (heavy top lid)
         if (r <= rOffset + 1) shape[r][c] = false;
         // Add a heavy blink effect occasionally during BORED
         if ((millis() / 2000) % 2 == 0 && r <= rOffset + 2) shape[r][c] = false; 
       }
    }
  }

  // Apply blink phase (Squint / Fully Closed) - Skip blinks for weather
  if (mood < 7 || mood >= 10) {
    if (blinkPhase == 1 || blinkPhase == 3) { // Squint
      for (int r = 0; r < 8; r++) {
         for (int c = 0; c < 6; c++) {
           if (r <= rOffset || r >= rOffset + height - 1) shape[r][c] = false;
         }
      }
    } else if (blinkPhase == 2) { // Fully closed
      for (int r = 0; r < 8; r++) {
         for (int c = 0; c < 6; c++) {
           if (r != rOffset + height / 2) shape[r][c] = false;
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
      for(int r=0; r<8; r++) for(int c=0; c<6; c++) shape[r][c] = false;
      
      // Draw scrolling "zzzzZZZZ" only once (for left eye call)
      if (isLeft) {
        int scrollX = 32 - ((millis() / 150) % 64); // Scroll right to left
        // Draw small 'z'
        auto drawZ = [&](int x, int y, bool big) {
           // FIXED: Use correct inversion for the Z characters (user said they were inverted)
           if (big) { // 5x5 Z
             for(int i=0; i<5; i++) { draw(y, x+i, true); draw(y+4, x+i, true); draw(y+4-i, x+i, true); }
           } else { // 3x3 z
             for(int i=0; i<3; i++) { draw(y, x+i, true); draw(y+2, x+i, true); draw(y+2-i, x+i, true); }
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
    // Apply center-out Scaling by simply adjusting height/offset proportionally (simplified)
    if (scale < 0.9) finalY += 1;
  }

  // BORED logic: smooth organic scanning
  if (mood == 14) {
    finalX = (int)(sin(millis() / 3000.0) * 5.0 + cos(millis() / 1500.0) * 2.0); // Wavy scanning
  }

  // DANCE logic: Rhythmic bounce
  if (mood == 17) {
    finalX += (int)(sin(millis() / 400.0) * 4.0);
    finalY += (int)(fabs(cos(millis() / 400.0)) * -3.0);
  }

  // SING logic: Floating musical notes (hide eyes)
  if (mood == 18) {
    for(int r=0; r<8; r++) for(int c=0; c<6; c++) shape[r][c] = false;
    if (isLeft) {
      for (int i = 0; i < 2; i++) {
        int noteX = 28 - ((millis() / (250 + i * 50)) % 24);
        int noteY = 2 + (int)(sin((millis() + i * 1000) / 400.0) * 2.0);
        if (i == 0) { // Single note ♪
           draw(noteY, noteX, true); draw(noteY+1, noteX, true); draw(noteY+2, noteX, true); draw(noteY, noteX+1, true);
        } else { // Double note ♫
           draw(noteY, noteX, true); draw(noteY, noteX+2, true); draw(noteY+1, noteX, true); draw(noteY+1, noteX+2, true); draw(noteY, noteX+1, true);
        }
      }
    }
  }

  // STARS logic: Twinkling background (hide eyes)
  if (mood == 16) {
    for(int r=0; r<8; r++) for(int c=0; c<6; c++) shape[r][c] = false;
    if (isLeft) {
       for(int i=0; i<6; i++) {
         int sx = (i * 137 + millis()/100) % 32;
         int sy = (i * 257 + millis()/150) % 8;
         if ((millis() / (200 + i*50)) % 2 == 0) draw(sy, sx, true);
       }
    }
  }

  for (int r = 0; r < 8; r++) {
    for (int c = 0; c < 6; c++) {
      if (shape[r][c]) draw(r + finalY, startC + c + finalX, true);
    }
  }
}

void animRoboEyes() {
  if (roboBlinkPhase == 0) {
    if (random(100) < 5) roboBlinkPhase = 1;
  } else {
    roboBlinkPhase++;
    if (roboBlinkPhase > 3) roboBlinkPhase = 0;
  }
  
  if (!manualMood) {
    if (roboTimer <= 0) {
      // Pick random mood, but skip weather (7-9)
      int rMood = random(0, 10); // 0-9 range
      if (rMood >= 7) rMood += 3; // Skip 7,8,9 → becomes 10,11,12
      roboMood = rMood;
      roboTimer = random(30, 100);
    } else {
      roboTimer--;
    }
  }

  if (roboMoveTimer <= 0) {
    if (roboMood < 7 || roboMood >= 10) { 
      if (random(100) < 60) {
        roboEyeX = 0; roboEyeY = 0;
        roboMoveTimer = random(20, 50);
      } else {
        roboEyeX = random(-2, 3);
        roboEyeY = random(-1, 2);
        roboMoveTimer = random(10, 30);
      }
    } else {
      roboEyeX = 0; roboEyeY = 0;
      roboMoveTimer = 10;
    }
  } else {
    roboMoveTimer--;
  }

  drawRoboEye(6, roboMood, roboBlinkPhase, true);  // Left eye
  drawRoboEye(20, roboMood, roboBlinkPhase, false); // Right eye
}

// ==================== 12. DOG REDESIGN (mode a) ====================
void drawDog(int x, int state, int frame, int dir) {
  auto draw = [&](int r, int relC, bool on) {
    int drawC = x + (relC * dir);
    if (drawC >= 0 && drawC < 32 && r >= 0 && r < 8) mx.setPoint(r, drawC, on ? !isInverse : isInverse);
  };
  
  // Head
  draw(3, 5, true); draw(3, 6, true); draw(3, 7, true);
  draw(4, 5, true); draw(4, 6, true); draw(4, 7, true); draw(4, 8, true); 
  draw(2, 6, true); 
  draw(3, 8, false); 
  if (state == 0 && frame % 2 == 0) draw(5, 8, true); 
  
  // Floppy ear
  if (frame % 2 == 0) { draw(3, 4, true); draw(4, 4, true); }
  else { draw(4, 4, true); draw(5, 4, true); }
  
  // Body
  for(int r=5; r<=6; r++) for(int c=1; c<=6; c++) draw(r, c, true);
  
  // Tail
  if (state == 0 || state == 2 || state == 4) { 
    if (frame % 2 == 0) { draw(3, 0, true); draw(4, 0, true); }
    else                { draw(4, 0, true); draw(5, -1, true); }
  } else { draw(4, 0, true); }

  // Legs & Context
  if (state == 0) { 
    draw(6, 1, false); draw(7, 2, true); draw(7, 5, true); draw(7, 6, true);
  } else if (state == 1) { 
    if (frame % 2 == 0) { draw(7, 2, true); draw(7, 5, true); }
    else { draw(7, 1, true); draw(7, 6, true); }
  } else if (state == 4) { 
    draw(6, 1, false); draw(7, 2, true); draw(7, 5, true);
    if (frame % 4 < 2) draw(1, 9, true); else draw(0, 10, true);
  } else if (state == 5) { 
    draw(7, 1, true); draw(7, 3, true); 
    if (frame % 2 == 0) { draw(6, 9, true); draw(7, 10, true); }
    else { draw(5, 10, true); draw(7, 9, true); }
  } else { 
    draw(7, 2, true); draw(7, 5, true);
  }
}

void animDog() {
  dogAnimFrame++;
  if (dogTimer <= 0) {
    if (dogState == 3 && poopX != -1) {
       dogState = 1; dogTargetX = constrain(poopX + 12, 0, 20); dogTimer = 30;
    } else {
       int next = random(0, 100);
       if (next < 30) { dogState = 1; dogTargetX = random(0, 20); dogTimer = random(30, 80); }
       else if (next < 50) {
         dogState = 2; dogTimer = random(50, 120);
         ballX = dogX + (dogDir * 12); if (ballX > 28 || ballX < 0) ballX = dogX - (dogDir * 8);
         ballY = 2; ballVX = (ballX > dogX) ? -1 : 1; ballVY = 1;
       } else if (next < 60) { dogState = 3; dogTimer = 25; poopX = -1; }
       else if (next < 75) { dogState = 4; dogTimer = random(40, 100); }
       else if (next < 85) { dogState = 5; dogTimer = random(30, 60); }
       else { dogState = 0; dogTimer = random(30, 60); }
    }
  }
  dogTimer--;

  if (dogState == 1) { 
    if (dogX < dogTargetX) { dogX++; dogDir = 1; } 
    else if (dogX > dogTargetX) { dogX--; dogDir = -1; } 
    else dogState = 0; 
  }
  if (dogState == 3 && dogTimer == 10) poopX = dogX - (dogDir * 2);
  if (dogState == 2) {
    if (ballX > dogX + 5) dogDir = 1; else if (ballX < dogX - 5) dogDir = -1;
    ballX += ballVX; ballY += ballVY;
    if (ballY >= 6) { ballY = 6; ballVY = -1; }
    if (ballY <= 1) { ballY = 1; ballVY = 1; } 
    if (ballX <= 0 || ballX >= 30) ballVX *= -1;
    int mouthX = dogX + (dogDir * 9);
    if (mouthX < ballX) dogX++; else if (mouthX > ballX) dogX--;
    if (abs(ballX - mouthX) <= 2 && ballY >= 3) { ballVX = (ballX > dogX) ? 1 : -1; ballVY = -1; }
  } else { ballX = -1; }

  if (poopX >= 0 && poopX < 32) {
    mx.setPoint(7, poopX, !isInverse); mx.setPoint(7, poopX+1, !isInverse); mx.setPoint(6, poopX+1, !isInverse);
  }
  if (ballX >= 0 && ballX < 31 && ballY >= 0 && ballY < 7) {
    mx.setPoint(ballY, ballX, !isInverse); mx.setPoint(ballY+1, ballX, !isInverse);
    mx.setPoint(ballY, ballX+1, !isInverse); mx.setPoint(ballY+1, ballX+1, !isInverse);
  }
  drawDog(dogX, dogState, dogAnimFrame, dogDir);
}



// ==================== NETWORK EVENT HANDLER ====================
void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_DATA) {
    AwsFrameInfo *info = (AwsFrameInfo*)arg;
    if (info->final && info->index == 0 && info->len == len && info->opcode == WS_TEXT) {
      data[len] = 0;
      String msg = (char*)data;
      if (msg.startsWith("cmd:mode:")) {
        String modeStr = msg.substring(9);
        if (modeStr == "2") currentMode = 2;
        else if (modeStr == "a") currentMode = 12;
        else if (modeStr == "99") currentMode = 99;
      } else if (msg.startsWith("cmd:mood:")) {
        String moodStr = msg.substring(9);
        if (moodStr == "auto") manualMood = false;
        else { roboMood = moodStr.toInt(); manualMood = true; currentMode = 2; }
      } else {
        networkText = msg;
        currentMode = 99;
      }
    }
  }
}

// --- Game state (Bounce) ---
float ballQX=16.0, ballQY=4.0, ballQVX=1.1, ballQVY=0.7;

void drawBounceGame() {
  // Update ball position
  ballQX += ballQVX; ballQY += ballQVY;

  // Bounce across full screen (0 to 31)
  if (ballQX <= 0.0)  { ballQX = 0.0;  ballQVX =  std::abs(ballQVX); }
  if (ballQX >= 31.0) { ballQX = 31.0; ballQVX = -std::abs(ballQVX); }
  if (ballQY <= 0.0)  { ballQY = 0.0;  ballQVY =  std::abs(ballQVY); }
  if (ballQY >= 7.0)  { ballQY = 7.0;  ballQVY = -std::abs(ballQVY); }
  
  // Draw ball and a short trail (no eye — full screen game)
  int bx = (int)ballQX;
  int by = (int)ballQY;
  mx.setPoint(by, bx, !isInverse);
  
  // Trail (previous position approximation)
  int tx = (int)(ballQX - ballQVX);
  int ty = (int)(ballQY - ballQVY);
  if (tx >= 0 && tx <= 31 && ty >= 0 && ty <= 7) {
    mx.setPoint(ty, tx, !isInverse);
  }
}

void animNetwork() {
  // Scroll text across the matrix
  if (networkText.length() == 0) {
    // Fallback to simple indicator
    if ((millis() / 500) % 2 == 0) {
      mx.setPoint(0, 0, !isInverse);
      mx.setPoint(0, 31, !isInverse);
    }
    return;
  }

  // Scroll speed
  if ((millis() / 200) % 1 == 0) { // Every 200ms
    scrollPos++;
    if (scrollPos > networkText.length() * 6 + 32) scrollPos = 0; // Reset after scrolling off
  }

  // Draw scrolling text
  for (int i = 0; i < networkText.length(); i++) {
    char c = networkText[i];
    int charStart = i * 6 - scrollPos;
    if (charStart > 31 || charStart < -5) continue;

    // Simple 5x7 font for each char
    for (int r = 0; r < 7; r++) {
      for (int b = 0; b < 5; b++) {
        if (charStart + b >= 0 && charStart + b < 32) {
          bool pixel = false;
          // Basic font mapping (simplified)
          if (c >= '0' && c <= '9') {
            int digit = c - '0';
            uint8_t font[10][5] = {
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
            pixel = (font[digit][r] & (1 << (4 - b))) != 0;
          } else if (c == ':') {
            pixel = (r == 2 || r == 4) && b == 2;
          } else if (c == '%') {
            pixel = ((r == 0 || r == 6) && b < 4) || ((r == 1 || r == 5) && (b == 0 || b == 3)) || ((r == 2 || r == 4) && (b == 1 || b == 2)) || (r == 3 && b == 2);
          } else if (c == ' ') {
            pixel = false;
          } else {
            // Default to block for unknown chars
            pixel = true;
          }
          if (pixel) mx.setPoint(r, charStart + b, !isInverse);
        }
      }
    }
  }
}

// ==================== SETUP ====================
void setup() {
  Serial.begin(115200);
  mx.begin();
  mx.control(MD_MAX72XX::INTENSITY, intensity);
  mx.clear();

  Serial.print("WiFi connecting");
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  unsigned long wifiStart = millis();
  int dot = 0;
  while (WiFi.status() != WL_CONNECTED && millis() - wifiStart < 8000) {
    mx.setPoint(4, dot % 32, true);
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
    while (now < 100000 && millis() - ntpStart < 5000) { delay(300); now = time(nullptr); }
    Serial.println("NTP synced!");
    
    // Setup API
    ws.onEvent(onWsEvent);
    server.addHandler(&ws);
    
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
      request->send_P(200, "text/html", INDEX_HTML);
    });

    server.on("/cmd", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("c")){
        String cStr = request->getParam("c")->value();
        if(cStr.length() > 0) {
          char cmd = cStr.charAt(0);
          if (cmd == 'i' || cmd == 'I') isInverse = !isInverse;
          if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
          if (cmd == '-' || cmd == '_') { if (intensity > 0)  intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
        }
      }
      request->send(200, "text/plain", "Command executed");
    });
    
    server.on("/mood", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("set")){
        String moodStr = request->getParam("set")->value();
        if (moodStr == "auto") manualMood = false;
        int mVal = moodStr.toInt();
        if (mVal == 15) { currentMode = 15; }
        else { roboMood = mVal; manualMood = true; currentMode = 2; }
      }
      request->send(200, "text/plain", "Mood updated");
    });

    server.on("/mode", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("set")){
        String modeStr = request->getParam("set")->value();
        if (modeStr == "2") currentMode = 2;
        else if (modeStr == "12") currentMode = 12;
        else if (modeStr == "15") currentMode = 15;
        else if (modeStr == "a") currentMode = 12;
        else if (modeStr == "99") currentMode = 99;
      }
      request->send(200, "text/plain", "Mode updated");
    });
    
    server.on("/text", HTTP_GET, [](AsyncWebServerRequest *request){
      if(request->hasParam("msg")){
        networkText = request->getParam("msg")->value();
        currentMode = 99;
      }
      request->send(200, "text/plain", "Text updated");
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
    for(int r=4-h/2; r<=4+h/2; r++) {
      for(int c=6; c<11; c++) mx.setPoint(r, c, !isInverse);
      for(int c=20; c<25; c++) mx.setPoint(r, c, !isInverse);
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
  if (wifiConnected) { ws.cleanupClients(); }

  if (Serial.available() > 0) {
    char cmd = Serial.read();
    if (cmd == '2') currentMode = 2;
    if (cmd == 'a' || cmd == 'A') { currentMode = 12; dogState = 0; ballX = -1; poopX = -1; }
    if (cmd == 'i' || cmd == 'I') isInverse = !isInverse;
    if (cmd == '+' || cmd == '=') { if (intensity < 15) intensity++; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    if (cmd == '-' || cmd == '_') { if (intensity > 0)  intensity--; mx.control(MD_MAX72XX::INTENSITY, intensity); }
    mx.clear();
  }

  if (millis() - lastTick < (unsigned long)animDelay) return;
  lastTick = millis();
  flip = !flip;

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::OFF);
  for (int r = 0; r < 8; r++) for (int c = 0; c < 32; c++) mx.setPoint(r, c, isInverse);

  switch (currentMode) {
    case 2:  animRoboEyes();  break;
    case 12: animDog();       break;
    case 15: drawBounceGame(); break;
    case 99: animNetwork();   break;
  }

  mx.control(MD_MAX72XX::UPDATE, MD_MAX72XX::ON);
}