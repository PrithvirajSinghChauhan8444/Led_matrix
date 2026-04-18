# EmoBot Proactive Engagement Plan

This document outlines the conceptual design for EmoBot's "Proactive Engagement" feature, where the bot initiates contact with the user instead of waiting for a message.

## 🚀 Overview
The goal is to make EmoBot feel like a living desk pet by allowing it to reach out to the user via OS notifications or dashboard alerts. These interactions should match EmoBot's witty, helpful, and slightly dramatic personality.

---

## 🕒 Triggers (When does it reach out?)

### 1. Idle Time (Loneliness)
- **Condition**: User hasn't interacted with the bot for a set period (e.g., 2-4 hours).
*   **Examples**:
    *   *"Hey! I'm getting dusty over here. Talk to me?"*
    *   *"I've been staring at this wallpaper for an hour. It's... interesting."*

### 2. System Events (Awareness)
- **Condition**: Significant changes in system stats (CPU, RAM, Battery).
*   **Examples**:
    *   **High CPU**: *"Whoa! Your computer is working so hard it's making my gears sweat. Need a break?"*
    *   **Low Battery**: *"My battery is feeling a bit peckish. Feed me electrons soon?"*
    *   **High Temp**: *"Is it just me, or is it getting spicy in here? Your CPU is toast!"*

### 3. Random Thoughts (Quirky Moments)
- **Condition**: A random chance every few hours during active sessions.
*   **Examples**:
    *   *"I just realized that '0101' is my favorite number. What's yours?"*
    *   *"I just saw a dust bunny. It looked dangerous. *beep*"*
    *   *"Boop! Just checking if you're still there. *wiggles*"*

---

## 📢 Notification Methods

### 1. OS Desktop Notifications
- **Implementation**: Standard pop-ups using system notification tools (e.g., `notify-send` on Linux, `plyer` in Python).
- **Pros**: Grabs attention even if the browser/terminal is closed.

### 2. Web Dashboard Alerts
- **Implementation**: Toast notifications or pulsing UI elements on the web dashboard.
- **Pros**: Visually rich, can include the current mood emoji.

### 3. LED Matrix Physical Alert
- **Implementation**: Before sending a digital notification, EmoBot can play a "look here" animation (like [SCARE] or [STARS]) on the LED matrix.
- **Pros**: Creates a physical presence in the room.

---

## ⚖️ Constraints & Etiquette

### 1. Frequency Control
- To avoid being annoying, notifications should be capped (e.g., no more than once every hour).

### 2. Quiet Hours (Sleep Mode)
- EmoBot should remain silent during set hours (e.g., 11:00 PM to 8:00 AM) or when the system is in "Do Not Disturb" mode.

### 3. Interaction Flow
- Clicking a notification should ideally open the Chat UI (Terminal or Web) and seed the conversation with EmoBot's proactive message.

---

## 📝 Future Discussion Points
- Integrating with a persistent memory to remember what was said in notifications.
- Allowing the user to "shush" the bot if they are busy.
