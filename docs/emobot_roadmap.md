# EmoBot — Feature Roadmap & Design Notes

## 1. Ambient Mood State Machine

### Problem
When the user is not chatting, EmoBot behaves randomly. It should drift through natural states like a real desktop pet (idle → bored → sleepy).

### Design

EmoBot has an **internal state** that changes based on:
- **Time since last interaction** (idle drift)
- **Time of day** (night = sleepy faster)
- **Last chat emotion** (carry-over mood)

#### State Transitions

| Time Since Last Chat | State |
|---|---|
| 0 – 30s | Current chat emotion (held) |
| 30s – 2min | `NORMAL` (idle, calm) |
| 2min – 5min | `BORED` (slow blinks) |
| 5min+ | `SLEEP` |

> **Note:** These durations are intentionally short for a desktop pet feel. Tune them via constants in code.

#### Time-of-Day Modifiers

| Time | Effect |
|---|---|
| 11pm – 6am | Reach `SLEEP` 2× faster |
| 6am – 9am | Default to `HAPPY` on wake |
| Rest of day | Normal drift |

### Implementation Plan

1. **Background thread** in `app.py` / `ollama_bot.py` that runs every ~10s.
2. Thread checks `time.time() - last_interaction_time`.
3. Based on elapsed time, calls `set_esp32_mood()` with the appropriate state.
4. Each new chat message updates `last_interaction_time` and resets the drift.

```python
# Pseudocode
import threading, time

last_interaction = time.time()
current_idle_mood = "NORMAL"

def idle_loop():
    while True:
        elapsed = time.time() - last_interaction
        if elapsed > 300:       # 5 min
            set_esp32_mood("SLEEP")
        elif elapsed > 120:     # 2 min
            set_esp32_mood("BORED") # TODO: add BORED to mood map
        elif elapsed > 30:      # 30 sec
            set_esp32_mood("NORMAL")
        time.sleep(10)

thread = threading.Thread(target=idle_loop, daemon=True)
thread.start()
```

---

## 2. Live Animation Streaming (AI → ESP32)

### Problem
Currently, the AI only tells the ESP32 *which mood* to display (a single integer). The ESP32 chooses the animation. We want the Python/AI layer to be in **full control** of what is rendered, frame by frame.

### Architecture

```
Ollama Model  →  Python  →  HTTP POST /frame  →  ESP32  →  LED Matrix
```

Python becomes the animation engine. The ESP32 is just a "dumb" renderer that draws whatever frame it receives.

### Frame Format

A frame is a flat array of pixel values, one per LED. For a 32×8 matrix = 256 values.

```json
POST /frame
Content-Type: application/json

{
  "frame": [0, 1, 0, 1, 0, 0, 1, 1, ...]
}
```

Each value: `0` = off, `1` = on (for monochrome), or a brightness value `0–255`.

### ESP32 Side (`main.cpp`)

Add a new HTTP endpoint `/frame`:

```cpp
server.on("/frame", HTTP_POST, []() {
    String body = server.arg("plain");
    DynamicJsonDocument doc(4096);
    deserializeJson(doc, body);
    JsonArray pixels = doc["frame"].as<JsonArray>();

    int i = 0;
    for (int y = 0; y < MATRIX_HEIGHT; y++) {
        for (int x = 0; x < MATRIX_WIDTH; x++) {
            int val = pixels[i++];
            matrix.drawPixel(x, y, val ? LED_ON : LED_OFF);
        }
    }
    matrix.show();
    server.send(200, "text/plain", "ok");
});
```

### Python Side

Define animations as lists of frames. Play them in a background thread.

```python
HAPPY_ANIM = [
    [0,1,0,1,0,0,0,0, ...],  # frame 1 (smile)
    [0,1,1,1,0,0,0,0, ...],  # frame 2 (big smile)
]

def play_animation(frames, loops=3, fps=8):
    for _ in range(loops):
        for frame in frames:
            requests.post(f"http://{ESP32_IP}/frame", json={"frame": frame}, timeout=0.5)
            time.sleep(1 / fps)

# Call after AI responds with a mood:
threading.Thread(target=play_animation, args=(HAPPY_ANIM,), daemon=True).start()
```

### AI Advantage
Since Python controls the renderer, the model can eventually:
- Generate its own frame patterns for new emotions
- Blend animations based on conversation intensity
- Update the face in real time mid-response (not just after)

### TODO (Implementation Order)
- [ ] Add `/frame` POST endpoint to ESP32 firmware
- [ ] Define matrix dimensions as constants (width × height)
- [ ] Define Python frame arrays for each emotion in `animations.py`
- [ ] Integrate `play_animation()` into `set_esp32_mood()`
- [ ] Run animation playback in a daemon thread to avoid blocking chat

---

## Summary

| Feature | Status | Priority |
|---|---|---|
| Mood state machine (idle drift) | Planned | High |
| Time-of-day modifier | Planned | Medium |
| Live frame streaming endpoint | Planned | High |
| Python animation library | Planned | High |
| AI-generated frame patterns | Future | Low |
