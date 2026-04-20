# 🤖 EmoBot: The Intelligent LED Matrix Desktop Companion

EmoBot is an advanced, AI-powered desktop pet and system monitor for the **MAX7219 8x32 LED Matrix**. It combines embedded C++ (ESP32), a Flask-based web interface, and local LLM intelligence (Ollama) to create a lively, expressive mechanical companion.

---

## 🚀 Implemented Features

### 🧠 AI Personality (Ollama Integrated)
EmoBot is more than a display—it's a witty, quirky pet who lives on your desk.
*   **Emotional Reactions**: AI responses automatically trigger LED face changes (Happy, Sad, Angry, Suspicious, etc.).
*   **Real-World Agency**: The AI can check the system time and open websites in Firefox on your host machine.
*   **Hardware Control**: The AI can switch display modes (e.g., "Switch to Dog mode") and adjust LED brightness.

### 🐾 Integrated Animations (src/main.cpp)
The core firmware includes high-quality, frame-by-frame animations:
*   **RoboEyes**: A highly expressive face with 15+ moods and organic blinking.
*   **Interactive Dog**: A complex pet that walks, sits, plays ball, and even has "accidents" (poops).
*   **Bounce Game**: A physics-based ball bounce animation.
*   **Shout Mode**: Overriding scrolling text for important messages.

### 📊 Real-Time Stats Dashboard
*   **System Monitoring**: Live tracking of your host's **CPU usage**, **RAM**, **Temperature**, and **Battery** status.
*   **NTP Sync**: Automatically fetches precise time from the internet on boot.

### 🌐 Web Control Center
*   **Glassmorphism UI**: A modern, interactive web interface to control modes, moods, and orientation.
*   **AI Chat Interface**: Real-time streaming chat with EmoBot.
*   **Hardware Config**: Remote control for brightness, rotation (180°), and display mirroring (Flip H/V).

---

## 🛠 Hardware Requirement

*   **Microcontroller**: ESP32 (NodeMCU or similar)
*   **Display**: MAX7219 8x32 LED Matrix (4-in-1 FC-16 Module)
*   **Host**: Linux machine running Ollama and Flask.

### Wiring (ESP32)
| MAX7219 Pin | ESP32 Pin | Board Label |
| :--- | :--- | :--- |
| VCC | 5V / 3.3V | VIN / 3V3 |
| GND | GND | GND |
| DIN | GPIO 13 | D13 |
| CS | GPIO 27 | D27 |
| CLK | GPIO 14 | D14 |

---

## 📂 Project Structure

*   `src/main.cpp`: Unified firmware integrating the core animation engine and Web API.
*   `web/app.py`: Flask backend managing the Ollama integration, tool calling, and system monitoring.
*   `web/system_monitor.py`: Background thread for fetching host system metrics.
*   `ollama_bot.py`: A CLI-based version of the EmoBot chat interface.
*   `extra_ani/`: Standalone legacy animations (Car, Train, Shin-chan Eyes) for modular use.
*   `agent_tools.md`: Roadmap for future "agentic" lifestyle features.

---

## ⚡ Quick Start

1.  **Flash Firmware**: Update WiFi credentials in `src/main.cpp` and upload via PlatformIO.
2.  **Start AI**: Ensure Ollama is running with `llama3.2`.
3.  **Launch Backend**:
    ```bash
    pip install -r requirements.txt
    python web/app.py
    ```
4.  **Connect**: Open `http://localhost:5000` to interact with your EmoBot.

---

## 📜 License
Open-source and hackable. Build your own mechanical friend!

*Developed with ❤️ as a multi-layered AI & Hardware experiment.*
