# EmoBot Evolution Roadmap

> From a chatbot with a face → a **living desktop companion** that sees, feels, and acts.

---

## Phase 1: System Awareness ("The Senses")

Feed EmoBot real-time system stats via `psutil` so it **reacts to your machine's state**.

### Data Sources & Reactions

| Stat | Source | EmoBot Reaction |
|:---|:---|:---|
| **CPU Usage** | `psutil.cpu_percent()` | >80% → ANNOYED. "Who's hogging my brain?!" |
| **RAM Usage** | `psutil.virtual_memory()` | >85% → SICK. "I feel bloated... close tabs!" |
| **Battery** | `psutil.sensors_battery()` | <15% → SCARE. "I'M DYING! PLUG ME IN!" |
| **Disk Space** | `psutil.disk_usage('/')` | <10% free → SUSPICIOUS. "Where did the space go..." |
| **CPU Temp** | `psutil.sensors_temperatures()` | >75°C → ANGRY. "It's getting HOT in here!" |
| **GPU Temp** | `nvidia-smi` or `sensors` | >80°C → SCARE + flicker animation |
| **Network** | `psutil.net_io_counters()` | Disconnected → SAD. Fast → DANCE |
| **Time of Day** | `datetime` | Late night → SLEEP. Morning → HAPPY |
| **Uptime** | `psutil.boot_time()` | >12h → BORED. "Restart me maybe?" |

### Implementation
- Background thread polling every 10s
- Feed stats into the LLM context so EmoBot can reference them in conversation
- Autonomous mood triggers (e.g., battery critical → override mood to SCARE)
- Stats injected into system prompt as `SYSTEM STATUS: CPU 45%, RAM 72%, Battery 80%`

---

## Phase 2: Idle Dashboard ("The Body")

When EmoBot is idle, the **LED matrix shows system stats** instead of just sleeping eyes.

### Idle State Rotation
```
0-30s idle  → Normal eyes
30-120s     → System stats ticker on LED matrix
120-300s    → Bounce game / Bored scanning
300s+       → Sleep animation
```

### LED Matrix Stats Display
- Scroll system values across 32×8 matrix: `CPU:45% RAM:72% BAT:80%`
- Use existing `/text` endpoint to push formatted strings from Python
- Tiny bar graphs (4px high) for CPU/RAM using pixel art

### Flask Frontend Dashboard
- Add a "System" panel to `index.html` showing:
  - CPU/RAM/Disk as progress bars
  - Battery with charge indicator
  - Network up/down speed
  - CPU/GPU temps with color coding (green/yellow/red)
  - System uptime
- Auto-refresh every 5s alongside emotion metrics
- New API endpoint: `GET /system_stats`

---

## Phase 3: Agentic System ("The Hands")

Give EmoBot the ability to **do things** on your PC via structured tool-calling.

### Safe Actions (no confirmation needed)
- Open applications: `firefox`, `code`, `nautilus`
- System notifications: `notify-send` on Linux
- Read clipboard, get time/date
- List files in a directory
- Take a screenshot
- Set a timer/reminder

### Guarded Actions (confirmation required)
- Kill a process by name/PID
- Write/move/delete files
- Change system volume/brightness
- Run arbitrary shell commands

### Architecture
```
User input → app.py → Ollama (with tool definitions)
                         ↓
                    Structured output:
                    [HAPPY][ACTION:open_app:firefox] Opening Firefox for you!
                         ↓
                    tool_executor.py → Execute + return result
                         ↓
                    Feed result back to Ollama for next response
```

### Safety Layer
- `safe: False` tools show a confirmation modal in the Flask UI
- Audit log with timestamp + tool + params + result
- Configurable whitelist/blacklist for apps and commands

---

## Phase 4: Personality Memory ("The Soul")

- **Persistent emotion history** — Save `emotion_stats` to JSON on disk
- **User preferences** — Remember apps, active hours
- **Conversation memory** — Summarize past chats into prompt
- **Learned behaviors** — "You always check email at 9am, want me to open it?"

---

## Proposed File Structure

```
Led_matrix/
├── src/main.cpp              # ESP32 firmware
├── ollama_bot.py             # CLI chatbot
├── web/
│   ├── app.py                # Flask backend (brain)
│   ├── system_monitor.py     # NEW: psutil stats collector
│   ├── tool_executor.py      # NEW: agentic action handlers
│   ├── static/ & templates/
├── data/
│   ├── emotion_history.json  # Persistent emotion stats
│   └── action_log.json       # Audit log
└── docs/
    └── emobot_roadmap.md     # This file
```

## Priority: Phase 1 → Phase 2 → Phase 3 → Phase 4
