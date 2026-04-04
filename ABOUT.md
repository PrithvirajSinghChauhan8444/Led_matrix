# 🚀 LED Matrix Controller (ESP8266)

A versatile, interactive animation suite for an **8x32 MAX7219 LED Matrix** powered by a **NodeMCU (ESP8266)**. This project features a variety of pixel-art animations, a real-time morphing clock, and interactive elements controlled via Serial.

---

## 🛠 Hardware Requirement
- **Microcontroller**: NodeMCU v2 (ESP8266)
- **Display**: MAX7219 8x32 LED Matrix (FC-16 style, 4 modules)
- **Connectivity**: WiFi (for NTP Time Sync)

---

## ✨ Features & Animations

### 🕒 Smart Clock
- **Morphing Clock**: Smooth vertical digit transitions.
- **Starfield Hybrid**: A screensaver mode with a clock toggle.
- **NTP Sync**: Automatically fetches precise time from the internet (IST by default).

### 🐾 Pixel Pets & Characters
- **Cat**: Idle, walking, sleeping, and pouncing animations.
- **Dog**: A highly interactive pet that walks, sits, wags its tail, plays ball, digs, and even poops!
- **Slime Pet**: A squishy companion that idles, squashes, and jumps.
- **Shin-chan Eyes**: Expressive eyes with multiple emotions and blinking.

### 🎮 Action & Scenery
- **Dino Fire Fight**: A 1v1 battle between two dinos with health bars, fireballs, and gravestones.
- **Stickman Action**: A "Madness Combat" style grunt performing idle and combat moves.
- **Bullet Train**: A large, detailed high-speed train passing through.
- **Sports Car**: A low-profile car with spinning wheels and road effects.

---

## ⌨️ Serial Controls

Open your Serial Monitor (115200 baud) to switch between modes:

| Command | Mode / Action |
| :-- | :-- |
| `0` | **Shin-chan Eyes** (Multiple emotions/mouths) |
| `1` | **Normal Eyes** (Classic blinking) |
| `2` | **Sports Car** (Scrolling road effect) |
| `3` or `s` | **Slime Pet** (Idle/Jump/Squash) |
| `4` | **Short Cat** (Walking/Sleeping/Pounce) |
| `7` | **Morphing Clock** (WiFi required) |
| `8` | **Stickman** (Action grunt) |
| `9` | **Bullet Train** (Speeding through) |
| `a` | **Interactive Dog** (Multi-state pet) |
| `e` | **Dino Fight** (Intercontinental Ballistic Dino) |
| `i` | **Inverse Mode** (Toggle Dark/Light) |
| `+` / `-` | **Brightness Up/Down** |

---

## 📂 Project Structure
- `src/main.cpp`: The unified controller with all modes integrated.
- `extra_ani/`: Standalone versions of individual animations for focused development or single-use flashing.
- `platformio.ini`: Project configuration and dependencies (`MD_Parola`, `MD_MAX72XX`).

---

## 🚀 Quick Start
1.  Open the project in **VS Code** with **PlatformIO** installed.
2.  Update your WiFi credentials in `src/main.cpp`:
    ```cpp
    const char* WIFI_SSID = "Your_SSID";
    const char* WIFI_PASS = "Your_Password";
    ```
3.  Connect your NodeMCU and hit **Upload**.
4.  Open the Serial Monitor and start sending commands!
