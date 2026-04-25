# ─── EmoBot Configuration ─────────────────────────────
# Change values here. No need to touch app.py or script.js.
# All times in seconds unless noted.

# ─── Idle State Machine ──────────────────────────────
IDLE_TIMEOUT_NORMAL = 30       # seconds before entering BORED
IDLE_TIMEOUT_BORED = 120       # seconds before entering SLEEP
IDLE_TIMEOUT_SLEEP = 300       # seconds before deep sleep dim

# ─── Proactive Notifications ─────────────────────────
PROACTIVE_ENABLED = True
PROACTIVE_TICK_INTERVAL = 60   # seconds between engine ticks
IDLE_LONELINESS_THRESHOLD = 1200  # 2 hours before lonely trigger
GLOBAL_COOLDOWN = 3600         # 1 hour min between notifications
QUIET_HOUR_START = 23          # 24h format
QUIET_HOUR_END = 23
RANDOM_QUIRK_CHANCE = 0.05     # 5% chance per tick

# ─── System Monitor Thresholds ───────────────────────
CPU_WARN_PERCENT = 85
RAM_WARN_PERCENT = 85
DISK_WARN_PERCENT = 10         # free space below this
CPU_TEMP_WARN_C = 90
GPU_TEMP_WARN_C = 80
BATTERY_LOW_PERCENT = 20

# ─── Animation Timings (Web Grid, milliseconds) ──────
BLINK_INTERVAL_MS = 4000       # time between blinks
BLINK_DURATION_MS = 150        # blink closed duration
EYE_DRIFT_MIN_MS = 800        # min time before eye moves
EYE_DRIFT_MAX_MS = 2500       # max time before eye moves
MOOD_CYCLE_MIN_TICKS = 30     # min ticks before auto mood change
MOOD_CYCLE_MAX_TICKS = 100    # max ticks

# ─── Expression Speeds (milliseconds) ────────────────
DIZZY_SPIRAL_SPEED_MS = 400
LOVE_PULSE_SPEED_MS = 400
CRY_TEAR_SPEED_MS = 200
THINK_DOT_SPEED_MS = 800
SNEEZE_CYCLE_MS = 2000
SNEEZE_BUILDUP_MS = 1500
EXCLAIM_FLASH_MS = 300
QUESTION_WOBBLE_MS = 500
DANCE_BOUNCE_MS = 400
SING_NOTE_SPEED_MS = 250
SLEEP_ZZZ_SCROLL_MS = 150
SLEEP_PHASE_MS = 30000
SICK_JITTER_MS = 50
ANNOYED_ROLL_MS = 150
BORED_SCAN_MS = 3000
SCARE_FLICKER_MS = 50
NAUGHTY_BREATHE_MS = 800

# ─── Stats Display ───────────────────────────────────
STATS_CYCLE_INTERVAL = 5       # seconds between stat rotations
STATS_SHOUT_DURATION = 3       # seconds to show stat on LED

# ─── Web Dashboard ───────────────────────────────────
MOOD_POLL_INTERVAL = 3         # seconds between mood polls
NOTIFICATION_POLL_INTERVAL = 5 # seconds between notif polls
TOAST_DISMISS_SECONDS = 8     # auto-dismiss toast after this

# ─── ESP32 Connection ────────────────────────────────
ESP32_IP = "10.42.0.145"
ESP32_TIMEOUT = 2              # seconds for HTTP timeout
SHOUT_DEFAULT_SPEED = 50       # scroll speed for shout text
SHOUT_DEFAULT_PAUSE = 4000     # ms pause after scroll

# ─── Feature specific ───────────────────────────────
WAKE_HOUR = 8
SLEEP_HOUR = 23
POSTURE_INTERVAL = 3600        # 1 hour
TREAT_MOOD = "HAPPY"           # Mood name for feeding
MOOD_FEED = 28                 # Special mood ID for feeding

# ─── New Visual Properties ──────────────────────────
SQUISH_SPEED = 1500            # ms per breathe cycle
SQUISH_STRENGTH = 0.5          # pixels of vertical stretch
AUDIO_PEAK_THRESHOLD = 1500    # Raw PCM peak threshold for dance
AUDIO_DANCE_DECAY = 0.8        # How fast the "jump" settles
