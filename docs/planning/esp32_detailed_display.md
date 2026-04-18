# Plan: Detailed Data Display for ESP32 (8x32 LED Matrix)

This plan outlines how to implement a "Nice & Detailed" data display function on the ESP32 to visualize computer stats or bot messages effectively on a small 8x32 grid.

---

## 🎨 Visual Design Concepts

### 1. Sectioned Layout (Static)
Divide the 32-pixel width into functional zones:
- **Zone 1 (0-7)**: Mini Icon (e.g., a tiny 'C' for CPU or a graph icon).
- **Zone 2 (8-31)**: The Value (e.g., "45%" or "72°C").
- *Benefit*: Instant readability without waiting for scrolls.

### 2. The "Dashboard" Cycle
Instead of showing everything at once, the ESP32 cycles through "Stat Cards":
- **Card 1**: CPU Icon + Percentage.
- **Card 2**: RAM Icon + Usage.
- **Card 3**: Temp Icon + Degrees.
*Transition*: Smooth "push" animation (old stat slides up, new one slides in from bottom).

### 3. Smart Scrolling
- If the data is short (e.g., "CPU: 20%"), it stays centered and static.
- If the data is long (e.g., "New Message: Hello Human!"), it automatically scrolls.

---

## 🛠 Proposed ESP32 Functionality

### 1. Data Protocol
We should send data in a structured format so the ESP32 knows what it is receiving.
- **Format**: `TYPE:VALUE` (e.g., `CPU:45`, `RAM:80`, `MSG:Hello`)
- **JSON**: `{ "t": "CPU", "v": 45 }` (Requires `ArduinoJson` library).

### 2. The `showDetailedData` Function (C++)
The function would handle:
1. **Icon Selection**: Mapping the `TYPE` to a custom 8x8 bitmap.
2. **Buffer Clearing**: Only clearing the text area if the icon stays the same.
3. **Rendering**: Using `MD_Parola` or `MD_MAX72XX` to draw the icon and text separately.

---

## 🛤 Implementation Steps

### Step 1: Define Icons
Create a library of 8x8 bitmaps for common system elements:
- ⚡ (Power/Battery)
- 🧠 (CPU/Brain)
- 📊 (RAM/Stats)
- 🌡️ (Temperature)
- ✉️ (Message)

### Step 2: New API Endpoint
Add a `/stats` or `/dashboard` endpoint to the ESP32 web server:
- `http://<IP>/stats?cpu=20&ram=50&temp=45`
- This allows the computer to send multiple values in one "heartbeat."

### Step 3: Animation Logic
Implement a "State Machine" on the ESP32 that:
1. Receives the data.
2. Stores it in local variables.
3. Every 3-5 seconds, updates the display to show the next stat in the list with a nice transition.

---

## ⚙️ Advanced Orientation Control
Since the LED module can be mounted in any orientation, we need software-level control to fix the output without rewiring.

### 1. Transformations
We will implement functions to handle the following:
- **Flip Horizontal**: Reverse the column order.
- **Flip Vertical**: Reverse the row order (top becomes bottom).
- **Mirroring**: Combinations of flips to handle rear-projection or complex mounting.
- **Rotation**: Support for 90°, 180°, and 270° rotations.

### 2. Configuration Endpoint
- **Endpoint**: `/config?flipV=0&flipH=1&rotate=0`
- **Persistence**: These settings should be saved in the ESP32's **LittleFS** or **EEPROM** so the display stays correct even after a reboot.

---

## 🌐 Web-to-LED Messaging
A dedicated feature for sending custom text directly from the Web Dashboard.

### 1. The "Shout" Feature
- A text box on the web UI where you type a message and hit "Send".
- **Endpoint**: `/shout?msg=Hello&speed=50&pause=2000`
- **Parameters**:
    - `msg`: The text to display.
    - `speed`: How fast the text scrolls.
    - `pause`: How long to wait after scrolling before returning to the previous mode (stats or clock).

---

## 📝 Discussion Questions
1. **Icons or Text?** Do you prefer having a small icon on the left, or just high-quality text?
2. **Speed**: How often should the stats cycle? (e.g., every 2 seconds vs. every 10 seconds).
3. **Persistence**: Should the orientation settings be saved permanently on the ESP32, or sent by the computer every time it starts up?
4. **Immediate Override**: When you type text on the web, should it immediately interrupt whatever EmoBot is doing, or wait for the current animation to finish?
