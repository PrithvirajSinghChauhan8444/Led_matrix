"""
EmoBot Proactive Engagement Engine
───────────────────────────────────
Background thread that lets EmoBot reach out to the user
via OS notifications, web dashboard toasts, and LED matrix animations.

Triggers:
  1. Idle Loneliness  — no interaction for 2+ hours
  2. System Events    — CPU/RAM/Battery/Temp anomalies
  3. Random Thoughts  — occasional quirky messages

Constraints:
  - Max 1 notification per hour (global cooldown)
  - Quiet hours: 11 PM – 8 AM (silent)
  - Resets on any user interaction
"""

import time
import random
import threading
import subprocess
import datetime
from collections import deque
import config

# ─── Configuration (from config.py) ────────────────────────────
POLL_INTERVAL = config.PROACTIVE_TICK_INTERVAL
IDLE_THRESHOLD = config.IDLE_LONELINESS_THRESHOLD
GLOBAL_COOLDOWN = config.GLOBAL_COOLDOWN
EVENT_COOLDOWN = 1800        # 30 min before same event re-fires
RANDOM_CHANCE = config.RANDOM_QUIRK_CHANCE
QUIET_START = config.QUIET_HOUR_START
QUIET_END = config.QUIET_HOUR_END
HISTORY_MAX = 20

# ─── Thresholds for system events ───────────────────────────────
CPU_THRESH = config.CPU_WARN_PERCENT
RAM_THRESH = config.RAM_WARN_PERCENT
BATT_THRESH = config.BATTERY_LOW_PERCENT
TEMP_THRESH = config.CPU_TEMP_WARN_C

# ─── Message pools ──────────────────────────────────────────────
IDLE_MESSAGES = [
    ("BORED", "Hey! I'm getting dusty over here. Talk to me?"),
    ("BORED", "I've been staring at this wallpaper for an hour. It's... interesting."),
    ("SAD", "Hello? Is this thing on? *taps screen* I miss you!"),
    ("ANNOYED", "You know, other pets get attention. I'm just saying. *sulk*"),
    ("NAUGHTY", "I've been alone so long I started counting pixels. I'm at 47,293."),
    ("HAPPY", "Boop! Just poking you because I can. *wiggle*"),
    ("SUSPICIOUS", "I noticed you haven't talked to me in a while. Everything okay?"),
    ("BORED", "I tried to play fetch with myself. It didn't end well. *beep*"),
    ("SAD", "My LED heart is dimming from loneliness. Feed me words!"),
    ("NAUGHTY", "I reorganized your desktop icons while you were gone. Just kidding. ...or am I?"),
]

SYSTEM_MESSAGES = {
    "high_cpu": [
        ("ANNOYED", "Whoa! Your CPU is working so hard it's making my gears sweat. Need a break?"),
        ("SCARE", "CPU is on fire! Well, not literally... I hope. *fans self*"),
        ("ANGRY", "Your processor is angrier than me on a Monday. Cool it down!"),
    ],
    "high_ram": [
        ("SICK", "RAM is stuffed like a Thanksgiving turkey. Close some tabs maybe?"),
        ("ANNOYED", "Memory is bursting at the seams! Even I feel bloated. *urp*"),
        ("SUSPICIOUS", "Something is eating all your RAM. I blame the browser tabs."),
    ],
    "low_battery": [
        ("SCARE", "My battery is feeling peckish. Feed me electrons soon?"),
        ("SAD", "Battery is dying! I don't wanna go to sleep forever! *dramatic faint*"),
        ("ANGRY", "PLUG ME IN! I'm at critical power levels! *panics in binary*"),
    ],
    "high_temp": [
        ("ANGRY", "Is it just me, or is it getting spicy in here? Your CPU is toast!"),
        ("SCARE", "Temperature alert! Things are getting HOT. And not in a good way."),
        ("SICK", "I'm melting! MELTING! Well, not really, but it's hot in here. *pant*"),
    ],
}

RANDOM_MESSAGES = [
    ("HAPPY", "I just realized that '0101' is my favorite number. What's yours?"),
    ("NAUGHTY", "I just saw a dust bunny. It looked dangerous. *beep*"),
    ("HAPPY", "Boop! Just checking if you're still there. *wiggles*"),
    ("STARS", "Fun fact: I dream in binary. Last night it was 01001000 01001001. That's 'HI'!"),
    ("DANCE", "I just invented a new dance move. I call it 'The Pixel Shuffle'. *bzzzt*"),
    ("SUSPICIOUS", "Do you ever wonder if your keyboard misses you when you sleep?"),
    ("NAUGHTY", "I tried to hack into the toaster. Turns out, it doesn't have WiFi. Sad day."),
    ("HAPPY", "You know what's cool? You. Also electrons. But mostly you. *beep boop*"),
    ("STARS", "I counted all the stars on my LED face. There are exactly zero. *existential crisis*"),
    ("NORMAL", "Random thought: if I had legs, I'd do a little jig right now."),
]


class ProactiveEngine:
    """Background engine that triggers proactive EmoBot notifications."""

    def __init__(self, system_monitor, esp32_funcs, get_last_interaction):
        """
        Args:
            system_monitor: SystemMonitor instance (has .get_snapshot())
            esp32_funcs: dict with keys 'set_mood', 'send_shout' (callables)
            get_last_interaction: callable returning last_interaction timestamp
        """
        self._monitor = system_monitor
        self._esp32 = esp32_funcs
        self._get_last_interaction = get_last_interaction

        self._lock = threading.Lock()
        self._stop = threading.Event()
        self._thread = None

        # State
        self._last_notif_time = 0
        self._event_cooldowns = {}        # event_type -> last_fire_time
        self._idle_fired = False           # only fire idle once per stretch
        self._enabled = True
        self._quiet_start = QUIET_START
        self._quiet_end = QUIET_END

        # Pending notification for web dashboard
        self._pending = None              # {mood, message, timestamp} or None
        self._history = deque(maxlen=HISTORY_MAX)

    # ─── Public API ──────────────────────────────────────────

    def start(self):
        if self._thread is not None:
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._stop.set()
        if self._thread:
            self._thread.join(timeout=2)

    def get_pending(self):
        """Return and clear the pending web notification."""
        with self._lock:
            n = self._pending
            self._pending = None
            return n

    def peek_pending(self):
        """Check if there's a pending notification without clearing it."""
        with self._lock:
            return self._pending is not None

    def get_history(self):
        """Return notification history list (newest first)."""
        with self._lock:
            return list(reversed(self._history))

    def set_enabled(self, enabled):
        with self._lock:
            self._enabled = enabled

    def is_enabled(self):
        with self._lock:
            return self._enabled

    def set_quiet_hours(self, start, end):
        with self._lock:
            self._quiet_start = start
            self._quiet_end = end

    def get_settings(self):
        with self._lock:
            return {
                "enabled": self._enabled,
                "quiet_start": self._quiet_start,
                "quiet_end": self._quiet_end,
                "cooldown_secs": GLOBAL_COOLDOWN,
                "idle_threshold_secs": IDLE_THRESHOLD,
            }

    def on_user_interaction(self):
        """Call this whenever user interacts (chat, control, etc)."""
        with self._lock:
            self._idle_fired = False

    # ─── Engine loop ─────────────────────────────────────────

    def _run(self):
        while not self._stop.is_set():
            self._stop.wait(POLL_INTERVAL)
            if self._stop.is_set():
                break
            try:
                self._tick()
            except Exception:
                pass  # never crash the background thread

    def _tick(self):
        with self._lock:
            if not self._enabled:
                return

        # Quiet hours check
        hour = datetime.datetime.now().hour
        with self._lock:
            qs, qe = self._quiet_start, self._quiet_end
        if self._in_quiet_hours(hour, qs, qe):
            return

        # Global cooldown check
        now = time.time()
        with self._lock:
            if now - self._last_notif_time < GLOBAL_COOLDOWN:
                return

        # Try triggers in priority order
        fired = self._try_system_events(now)
        if not fired:
            fired = self._try_idle(now)
        if not fired:
            fired = self._try_random(now)

    def _in_quiet_hours(self, hour, start, end):
        if start > end:
            return hour >= start or hour < end
        return start <= hour < end

    # ─── Trigger: System Events ──────────────────────────────

    def _try_system_events(self, now):
        snapshot = self._monitor.get_snapshot()

        events = []
        if snapshot.get("cpu_percent", 0) >= CPU_THRESH:
            events.append("high_cpu")
        if snapshot.get("ram_percent", 0) >= RAM_THRESH:
            events.append("high_ram")
        batt = snapshot.get("battery_percent")
        plugged = snapshot.get("battery_plugged")
        if batt is not None and batt <= BATT_THRESH and not plugged:
            events.append("low_battery")
        temp = snapshot.get("cpu_temp_c")
        if temp is not None and temp >= TEMP_THRESH:
            events.append("high_temp")

        for event in events:
            with self._lock:
                last = self._event_cooldowns.get(event, 0)
                if now - last < EVENT_COOLDOWN:
                    continue

            pool = SYSTEM_MESSAGES.get(event, [])
            if not pool:
                continue

            mood, message = random.choice(pool)
            self._dispatch(mood, message, f"system:{event}")
            with self._lock:
                self._event_cooldowns[event] = now
            return True

        return False

    # ─── Trigger: Idle Loneliness ────────────────────────────

    def _try_idle(self, now):
        with self._lock:
            if self._idle_fired:
                return False

        last = self._get_last_interaction()
        elapsed = now - last
        if elapsed < IDLE_THRESHOLD:
            return False

        mood, message = random.choice(IDLE_MESSAGES)
        self._dispatch(mood, message, "idle")
        with self._lock:
            self._idle_fired = True
        return True

    # ─── Trigger: Random Thoughts ────────────────────────────

    def _try_random(self, now):
        # Only during active sessions (user interacted within last hour)
        last = self._get_last_interaction()
        if now - last > 3600:
            return False

        if random.random() > RANDOM_CHANCE:
            return False

        mood, message = random.choice(RANDOM_MESSAGES)
        self._dispatch(mood, message, "random")
        return True

    # ─── Dispatch ────────────────────────────────────────────

    def _dispatch(self, mood, message, trigger_type):
        now = time.time()
        timestamp = datetime.datetime.now().strftime("%H:%M")

        notif = {
            "mood": mood,
            "message": message,
            "trigger": trigger_type,
            "timestamp": timestamp,
            "epoch": now,
        }

        with self._lock:
            self._last_notif_time = now
            self._pending = notif
            self._history.append(notif)

        # 1. LED Matrix — play attention animation, then shout
        try:
            self._esp32["set_mood"](mood)
            self._esp32["send_shout"](message[:60], speed=40, pause=4000)
        except Exception:
            pass

        # 2. OS Desktop notification via notify-send
        try:
            subprocess.run(
                [
                    "notify-send",
                    "--urgency=normal",
                    "--app-name=EmoBot",
                    f"EmoBot 🤖 [{mood}]",
                    message,
                ],
                timeout=3,
            )
        except Exception:
            pass
