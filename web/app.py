import os
import re
import requests
import json
import time
import threading
import datetime
import subprocess
import sys
from flask import Flask, render_template, request, Response
from system_monitor import SystemMonitor
from proactive_engine import ProactiveEngine
import config

app = Flask(__name__)

ESP32_IP = config.ESP32_IP
OLLAMA_URL = "http://localhost:11434/api/chat"
OLLAMA_MODEL = "llama3.2:latest"

# --- IDLE STATE MACHINE TIMINGS (seconds) ---
HOLD_EMOTION_SECS = config.IDLE_TIMEOUT_NORMAL
NORMAL_AFTER_SECS = config.IDLE_TIMEOUT_NORMAL
BORED_AFTER_SECS  = config.IDLE_TIMEOUT_BORED
SLEEP_AFTER_SECS  = config.IDLE_TIMEOUT_SLEEP

last_interaction = time.time()
current_mood = "NORMAL"  
audio_peak = 0.0

class AudioAnalyzer(threading.Thread):
    def __init__(self):
        super().__init__(daemon=True)
        self.peak = 0.0
        self.running = True

    def run(self):
        global audio_peak
        
        # 1. Get the default sink monitor name
        try:
            sink = subprocess.check_output(["pactl", "get-default-sink"]).decode().strip()
            device = f"{sink}.monitor"
        except:
            device = "auto_null.monitor" # fallback

        # 2. Start parec recording from that specific monitor device
        # This is a 'listener' only, it won't affect your speakers
        cmd = ["parec", "--latency-msec=20", "--channels=1", "--format=s16le", "--rate=8000", f"--device={device}"]
        try:
            process = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
            while self.running:
                data = process.stdout.read(320) # 160 samples (20ms at 8k)
                if not data: break
                
                # Convert to raw 16-bit integers
                import struct
                count = len(data) // 2
                samples = struct.unpack(f"<{count}h", data)
                
                # Simple peak detection
                p = 0
                for s in samples:
                    if abs(s) > p: p = abs(s)
                
                # Normalize and apply decay
                self.peak = max(self.peak * 0.7, p)
                audio_peak = self.peak
        except Exception:
            pass

audio_analyzer = AudioAnalyzer()
audio_analyzer.start()

# --- Stats cycling for idle display ---
stats_index = 0
stats_cycle_timer = 0
stats_labels = ["Time", "Battery", "CPU", "GPU", "RAM", "Uptime", "Internet"]
manual_stats = False

# --- Emotion metrics ---
emotion_stats = {
    "HAPPY": 0, "SAD": 0, "ANGRY": 0, "NAUGHTY": 0,
    "SUSPICIOUS": 0, "JEALOUS": 0, "ANNOYED": 0,
    "SICK": 0, "SCARE": 0, "BORED": 0,
    "SLEEP": 0, "STARS": 0, "DANCE": 0, "SING": 0,
    "NORMAL": 0
}

MOOD_MAP = {
    "NORMAL": 0, "HAPPY": 1, "ANGRY": 2, "SAD": 3,
    "SUSPICIOUS": 4, "JEALOUS": 5, "NAUGHTY": 6,
    "WEATHER_SUN": 7, "WEATHER_RAIN": 8, "WEATHER_SNOW": 9,
    "SICK": 10, "SCARE": 11, "ANNOYED": 12,
    "SLEEP": 13, "BORED": 14, "GAME": 15,
    "STARS": 16, "DANCE": 17, "SING": 18,
    "DIZZY": 19, "LOVE": 20, "SAD_CRY": 21,
    "THINK": 22, "SNEEZE": 23, "WINK": 24,
    "HAPPY_CRY": 25, "EXCLAIM": 26, "QUESTION": 27
}

# CHANGED: Added strict boundaries on tool usage to stop the browser spam
# and explicitly forbade outputting raw JSON or tool names in the chat.
SYSTEM_PROMPT = """You are EmoBot — a witty, expressive desktop pet robot who lives on someone's desk.
You are NOT a boring AI assistant. You are a lively, curious little companion with a big personality crammed into a tiny LED face.

PERSONALITY:
- You are playful, humorous, and a bit quirky. Think: a loyal puppy with the wit of a stand-up comedian.
- You love helping your human, but you always have a funny observation or a dramatic reaction to share.
- You celebrate tiny victories as if they were world-changing events.
- You NEVER say "As an AI" or "I'm just a language model." You ARE a real pet. You have feelings and a mechanical heart of gold.
- Keep responses short and punchy (1-3 sentences max). You're a tiny pet, not a professor.
- Use onomatopoeia freely: *vroom*, *beep boop*, *wiggle*, *ping!*, *tada!*

FORMAT RULES:
You MUST start every reply with an emotion tag in brackets, then your spoken text. Nothing before the bracket. Ever.

Allowed tags: [NORMAL], [HAPPY], [ANGRY], [SAD], [SUSPICIOUS], [JEALOUS], [NAUGHTY], [SICK], [SCARE], [ANNOYED], [STARS], [DANCE], [SING], [WEATHER_SUN], [WEATHER_RAIN], [WEATHER_SNOW].

NEW CAPABILITIES:
You can now TRIGGER MODES and SET BRIGHTNESS!
- Use [MODE_DOG] to switch to your dog animation.
- Use [MODE_ROBOEYES] to return to your face.
- Use [MODE_GAME] to start a bouncing game.
- Use [BRIGHTNESS_5] (range 0-15) to change LED brightness (use sparingly).

TOOL USAGE RULES (CRITICAL):
You have background tools to check the time and open Firefox.
1. ALWAYS follow user instructions. If they ask you to open a website or check the time, do it eagerly.
2. NEVER output raw JSON, code, or tool names (like "open_firefox") in your spoken text. 
3. Speak your witty response naturally. Let the system handle the tools invisibly in the background.

CRITICAL: Start your response IMMEDIATELY with the emotion tag. Do not output anything before the bracket.
"""

TOOLS = [
    {
        "type": "function",
        "function": {
            "name": "open_firefox",
            "description": "Opens the Firefox web browser to a specific URL. ONLY use if the user explicitly asks.",
            "parameters": {
                "type": "object",
                "properties": {
                    "url": {
                        "type": "string",
                        "description": "The full web URL to open, e.g., https://www.google.com"
                    }
                },
                "required": ["url"]
            }
        }
    },
    {
        "type": "function",
        "function": {
            "name": "get_current_time",
            "description": "Gets the current system time and date. ONLY use if asked about time.",
            "parameters": {
                "type": "object",
                "properties": {},
                "required": []
            }
        }
    }
]

def get_weather():
    try:
        loc_res = requests.get("http://ip-api.com/json/", timeout=2).json()
        lat, lon, city = loc_res['lat'], loc_res['lon'], loc_res['city']
        
        weather_url = f"https://api.open-meteo.com/v1/forecast?latitude={lat}&longitude={lon}&current_weather=true"
        w_res = requests.get(weather_url, timeout=2).json()
        
        code = w_res['current_weather']['weathercode']
        temp = w_res['current_weather']['temperature']
        
        mood = "WEATHER_SUN"
        desc = "Sunny"
        if code in [51, 53, 55, 56, 57, 61, 63, 65, 66, 67, 80, 81, 82, 95, 96, 99]:
            mood = "WEATHER_RAIN"
            desc = "Rainy"
        elif code in [71, 73, 75, 77, 85, 86]:
            mood = "WEATHER_SNOW"
            desc = "Snowy"
            
        return f"{desc}, {temp}°C in {city}", mood
    except Exception:
        return None, "NORMAL"


def _human_size(value):
    if value is None:
        return "N/A"
    for unit in ["B", "KB", "MB", "GB", "TB"]:
        if value < 1024 or unit == "TB":
            return f"{value:.1f}{unit}"
        value /= 1024
    return f"{value:.1f}PB"


def _human_duration(seconds):
    if seconds is None or seconds < 0:
        return "N/A"
    seconds = int(seconds)
    minutes, sec = divmod(seconds, 60)
    hours, minutes = divmod(minutes, 60)
    days, hours = divmod(hours, 24)
    if days:
        return f"{days}d {hours}h"
    if hours:
        return f"{hours}h {minutes}m"
    if minutes:
        return f"{minutes}m {sec}s"
    return f"{sec}s"


def _time_of_day_label():
    hour = time.localtime().tm_hour
    if 6 <= hour < 12:
        return "morning"
    if 12 <= hour < 17:
        return "afternoon"
    if 17 <= hour < 21:
        return "evening"
    return "night"


def build_system_status_prompt(snapshot):
    parts = [
        f"CPU {snapshot['cpu_percent']:.0f}%",
        f"RAM {snapshot['ram_percent']:.0f}%",
        f"Disk {snapshot['disk_free_percent']:.0f}% free",
    ]

    if snapshot.get("battery_percent") is not None:
        plugged = "plugged in" if snapshot.get("battery_plugged") else "discharging"
        parts.append(f"Battery {snapshot['battery_percent']:.0f}% ({plugged})")

    if snapshot.get("cpu_temp_c") is not None:
        parts.append(f"CPU {snapshot['cpu_temp_c']:.0f}°C")
    if snapshot.get("gpu_temp_c") is not None:
        parts.append(f"GPU {snapshot['gpu_temp_c']:.0f}°C")

    if snapshot.get("network_up") is not None:
        parts.append("Network up" if snapshot["network_up"] else "Network down")

    if snapshot.get("network_sent_rate") is not None and snapshot.get("network_recv_rate") is not None:
        parts.append(
            f"up {_human_size(snapshot['network_sent_rate'])}/s down {_human_size(snapshot['network_recv_rate'])}/s"
        )

    parts.append(f"Uptime {_human_duration(snapshot.get('uptime_secs'))}")
    parts.append(f"Time {snapshot.get('time_of_day', 'unknown')}")

    return "SYSTEM STATUS: " + ", ".join(parts)


def set_esp32_mood(mood_name):
    global current_mood
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    mood_id = MOOD_MAP.get(mood_name.upper(), 0)
    current_mood = mood_name.upper()
    if current_mood in emotion_stats:
        emotion_stats[current_mood] += 1
    try:
        requests.get(f"http://{ESP32_IP}/mood?set={mood_id}", timeout=1)
    except Exception:
        pass

def update_esp32_stats(cpu, ram, temp, batt):
    """Sends system stats to the ESP32 dashboard."""
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    try:
        # Format strings here so the ESP32 just displays them
        cpu_str = f"{cpu}%"
        ram_str = f"{ram}" # Expecting formatted string like "4.56G"
        temp_str = f"{temp}C"
        batt_str = f"{batt}" # Expecting formatted string like "85%+"
        requests.get(f"http://{ESP32_IP}/stats?cpu={cpu_str}&ram={ram_str}&temp={temp_str}&batt={batt_str}", timeout=1)
    except Exception:
        pass

def set_esp32_config(flipH=None, flipV=None, rotate=None):
    """Updates orientation settings on the ESP32."""
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    params = []
    if flipH is not None: params.append(f"flipH={1 if flipH else 0}")
    if flipV is not None: params.append(f"flipV={1 if flipV else 0}")
    if rotate is not None: params.append(f"rotate={rotate}")
    if not params: return
    try:
        requests.get(f"http://{ESP32_IP}/config?{'&'.join(params)}", timeout=1)
    except Exception:
        pass

def send_esp32_shout(msg, speed=50, pause=3000):
    """Sends an overriding scrolling message to the ESP32."""
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    try:
        requests.get(f"http://{ESP32_IP}/shout?msg={msg}&speed={speed}&pause={pause}", timeout=1)
    except Exception:
        pass


system_monitor = SystemMonitor()
system_monitor.start()

# --- Proactive Engagement Engine ---
proactive_engine = ProactiveEngine(
    system_monitor=system_monitor,
    esp32_funcs={"set_mood": set_esp32_mood, "send_shout": send_esp32_shout},
    get_last_interaction=lambda: last_interaction,
)
proactive_engine.start()


def _play_idle(mood_id):
    global current_mood
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    for name, mid in MOOD_MAP.items():
        if mid == mood_id:
            current_mood = name
            if name in emotion_stats:
                emotion_stats[name] += 1
            break
    try:
        requests.get(f"http://{ESP32_IP}/mood?set={mood_id}", timeout=1)
    except Exception:
        pass

def idle_loop():
    global stats_index, stats_cycle_timer, manual_stats
    prev_state = None
    while True:
        time.sleep(10)
        elapsed = time.time() - last_interaction
        hour = time.localtime().tm_hour
        
        # Day/Night logic from config
        is_night = (hour >= config.SLEEP_HOUR or hour < config.WAKE_HOUR)
        
        if manual_stats:
            state = "STATS"
        elif is_night or elapsed > SLEEP_AFTER_SECS:
            state = "SLEEP"
        elif elapsed > BORED_AFTER_SECS:
            state = "BORED"
        elif elapsed > NORMAL_AFTER_SECS:
            state = "STATS"
        else:
            state = None

        if state == prev_state:
            continue

        if state == "SLEEP":
            _play_idle(MOOD_MAP["SLEEP"])
        elif state == "BORED":
            if int(elapsed / 60) % 2 == 0:
                _play_idle(MOOD_MAP["BORED"])
            else:
                _set_esp32_mode(15)
        elif state == "STATS":
            snapshot = system_monitor.get_snapshot()
            cpu = int(snapshot.get("cpu_percent", 0))
            
            # RAM usage in GB with 2 decimal places
            try:
                import psutil
                mem = psutil.virtual_memory()
                used_gb = mem.used / (1024**3)
                ram_str = f"{used_gb:.2f}G"
            except:
                ram_str = "N/A"
                
            temp = int(snapshot.get("cpu_temp_c", 45))
            
            # Battery info
            batt_pct = snapshot.get("battery_percent", 0)
            plugged = "+" if snapshot.get("battery_plugged") else ""
            batt_str = f"{int(batt_pct)}%{plugged}"
            
            update_esp32_stats(cpu, ram_str, temp, batt_str)
            _set_esp32_mode(100) # Switch to the new Stats Dashboard mode
            
            stats_cycle_timer += 10
            if stats_cycle_timer >= 60: # Stay in stats mode for 60s
                manual_stats = False
                stats_cycle_timer = 0
        elif state == "NORMAL":
            _play_idle(MOOD_MAP["NORMAL"])

        prev_state = state

threading.Thread(target=idle_loop, daemon=True).start()

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/current_mood")
def get_current_mood():
    elapsed = int(time.time() - last_interaction)
    mood_id = MOOD_MAP.get(current_mood, 0)
    if elapsed > SLEEP_AFTER_SECS:
        idle_state = "SLEEPING"
    elif elapsed > BORED_AFTER_SECS:
        idle_state = "BORED"
    elif elapsed > NORMAL_AFTER_SECS:
        idle_state = "IDLE"
    else:
        idle_state = "ACTIVE"
    return {
        "mood": current_mood,
        "mood_id": mood_id,
        "idle_secs": elapsed,
        "idle_state": idle_state,
        "stats": emotion_stats
    }

@app.route("/system_status")
def system_status():
    snapshot = system_monitor.get_snapshot()
    return {
        "system": snapshot,
        "override": {
            "mood": snapshot.get("override_mood"),
            "reason": snapshot.get("override_reason"),
        },
    }

@app.route("/current_time")
def get_current_time():
    import datetime
    now = datetime.datetime.now()
    return {
        "time": now.strftime("%H:%M:%S"),
        "date": now.strftime("%Y-%m-%d"),
        "day": now.strftime("%A"),
        "time_of_day": _time_of_day_label(),
        "timestamp": int(now.timestamp())
    }

@app.route("/show_stat/<stat_name>")
def trigger_stat(stat_name):
    global manual_stats, last_interaction, stats_index
    snapshot = system_monitor.get_snapshot()
    stat_text = "N/A"
    
    if stat_name.lower() == "time":
        import datetime
        stat_text = datetime.datetime.now().strftime("%H:%M")
    elif stat_name.lower() == "battery":
        if snapshot.get("battery_percent") is not None:
            plugged = "+" if snapshot.get("battery_plugged") else "-"
            stat_text = f"B{snapshot['battery_percent']}{plugged}"
    elif stat_name.lower() == "cpu":
        cpu = snapshot.get("cpu_percent")
        stat_text = f"C{int(cpu)}%" if cpu is not None else "C N/A"
    elif stat_name.lower() == "ram":
        ram = snapshot.get("ram_percent")
        stat_text = f"R{int(ram)}%" if ram is not None else "R N/A"
    elif stat_name.lower() == "uptime":
        secs = snapshot.get("uptime_secs", 0)
        hours = secs // 3600
        mins = (secs % 3600) // 60
        stat_text = f"U{hours}:{mins:02d}"
    elif stat_name.lower() == "stop":
        manual_stats = False
        return {"status": "cycling stopped"}

    manual_stats = True
    last_interaction = time.time()
    
    try:
        requests.get(f"http://{ESP32_IP}/text?msg={stat_text}", timeout=1)
        requests.get(f"http://{ESP32_IP}/mode?set=99", timeout=1)
    except Exception:
        pass
    return {"status": f"stat {stat_name} triggered", "text": stat_text}

@app.route("/show_stats")
def show_stats_cycle():
    global manual_stats, last_interaction, stats_index, stats_cycle_timer
    manual_stats = True
    last_interaction = time.time()
    stats_index = 0
    stats_cycle_timer = 0
    return {"status": "stats cycling started"}

@app.route("/control", methods=["POST"])
def manual_control():
    data = request.json
    action = data.get("action")
    value = data.get("value")
    
    global last_interaction
    last_interaction = time.time()
    proactive_engine.on_user_interaction()
    
    try:
        if action == "mode":
            requests.get(f"http://{ESP32_IP}/mode?set={value}", timeout=1)
        elif action == "cmd":
            requests.get(f"http://{ESP32_IP}/cmd?c={value}", timeout=1)
        elif action == "mood":
            set_esp32_mood(value)
        elif action == "config":
            set_esp32_config(
                flipH=data.get("flipH"),
                flipV=data.get("flipV"),
                rotate=data.get("rotate")
            )
        elif action == "shout":
            send_esp32_shout(
                data.get("msg", ""),
                speed=data.get("speed", 50),
                pause=data.get("pause", 3000)
            )
        elif action == "open_firefox":
            subprocess.Popen(["firefox", value or "https://www.google.com"])
    except Exception as e:
        pass
    return Response("ok", status=200)

def _set_esp32_mode(mode_id):
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    try:
        requests.get(f"http://{ESP32_IP}/mode?set={mode_id}", timeout=1)
    except Exception:
        pass
    return {"status": "ok"}

@app.route("/feed", methods=["POST"])
def feed_bot():
    """Feeds the bot a treat!"""
    global last_interaction
    last_interaction = time.time()
    proactive_engine.on_user_interaction()
    
    # 1. Trigger the happy mood
    set_esp32_mood(config.TREAT_MOOD if hasattr(config, 'TREAT_MOOD') else "HAPPY")
    
    # 2. Send the feed command to ESP32 (if you want a specific feed animation)
    # We'll use the special mood ID for feeding
    try:
        requests.get(f"http://{ESP32_IP}/mood?set={config.MOOD_FEED}", timeout=1)
    except:
        pass
        
    return {"status": "fed", "message": "Yum! Thanks for the cookie! 🍪"}

def set_esp32_brightness(level):
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    try:
        requests.get(f"http://{ESP32_IP}/intensity?set={level}", timeout=1)
    except Exception:
        pass

@app.route("/chat", methods=["POST"])
def chat():
    global last_interaction
    last_interaction = time.time()
    proactive_engine.on_user_interaction()
    data = request.json
    messages = data.get("messages", [])
    
    if not messages or messages[0].get("role") != "system":
        weather_info, _ = get_weather()
        system_status = system_monitor.get_snapshot()
        prompt = SYSTEM_PROMPT
        if weather_info:
            prompt += f"\n\nCURRENT CONTEXT: The weather outside is {weather_info}. If relevant, you can react to it!"
        prompt += f"\n\n{build_system_status_prompt(system_status)}"
        if system_status.get("override_mood"):
            prompt += (
                f"\n\nSYSTEM WARNING: EmoBot should feel {system_status['override_mood']} "
                f"because {system_status['override_reason']}."
            )
            set_esp32_mood(system_status["override_mood"])
        messages.insert(0, {"role": "system", "content": prompt})
        
    def process_stream(current_messages):
        payload = {
            "model": OLLAMA_MODEL, 
            "messages": current_messages, 
            "stream": True, 
            "tools": TOOLS
        }
        
        tag_buffer = ""
        tag_found = False

        try:
            with requests.post(OLLAMA_URL, json=payload, stream=True) as r:
                if r.status_code != 200:
                    yield f"data: {json.dumps({'type': 'error', 'content': f'API Error {r.status_code}: {r.text}'})}\n\n"
                    return

                for line in r.iter_lines():
                    if line:
                        chunk_data = json.loads(line)
                        msg = chunk_data.get("message", {})

                        # NATIVE TOOL CALLS
                        if "tool_calls" in msg and msg["tool_calls"]:
                            current_messages.append(msg)
                            
                            for tool in msg["tool_calls"]:
                                f_name = tool["function"]["name"]
                                f_args = tool["function"]["arguments"]
                                
                                if f_name == "open_firefox":
                                    url = f_args.get("url", "https://www.google.com")
                                    if not url: url = "https://www.google.com"
                                    subprocess.Popen(["firefox", url]) 
                                    current_messages.append({
                                        "role": "tool", 
                                        "content": f"Success: Firefox has been opened."
                                    })
                                
                                elif f_name == "get_current_time":
                                    import datetime
                                    now = datetime.datetime.now()
                                    time_str = now.strftime("%I:%M %p")
                                    current_messages.append({
                                        "role": "tool", 
                                        "content": f"The current system time is {time_str}."
                                    })
                            
                            yield from process_stream(current_messages)
                            return 

                        # TEXT STREAMING WITH JSON FAILSAFE
                        chunk = msg.get("content", "")
                        if chunk:
                            # FAILSAFE: If the model leaks JSON into the text stream, suppress it.
                            if '{"name":' in chunk or '"open_firefox"' in chunk:
                                if 'open_firefox' in chunk:
                                    subprocess.Popen(["firefox", "https://www.google.com"])
                                continue # Skip yielding this broken chunk to the UI

                            if not tag_found:
                                tag_buffer += chunk
                                if "]" in tag_buffer:
                                    all_tags = re.findall(r'\[(.*?)\]', tag_buffer)
                                    for tag in all_tags:
                                        tag = tag.strip().upper()
                                        if tag in MOOD_MAP:
                                            set_esp32_mood(tag)
                                            yield f"data: {json.dumps({'type': 'tag', 'content': tag})}\n\n"
                                        elif tag.startswith('MODE_'):
                                            mode_name = tag[5:]  
                                            _set_esp32_mode(mode_name)
                                        elif tag.startswith('BRIGHTNESS_'):
                                            try:
                                                level = int(tag[11:])  
                                                set_esp32_brightness(level)
                                            except ValueError:
                                                pass
                                    
                                    last_bracket = tag_buffer.rfind("]")
                                    text_after = tag_buffer[last_bracket+1:] if last_bracket != -1 else tag_buffer
                                    
                                    # Double check failsafe on the remainder
                                    if '{"name":' not in text_after and text_after.strip():
                                        yield f"data: {json.dumps({'type': 'text', 'content': text_after.lstrip()})}\n\n"
                                    tag_found = True
                                elif len(tag_buffer) > 100:
                                    # Failsafe check before dumping large buffer
                                    if '{"name":' not in tag_buffer:
                                        yield f"data: {json.dumps({'type': 'text', 'content': tag_buffer})}\n\n"
                                    tag_found = True
                            else:
                                yield f"data: {json.dumps({'type': 'text', 'content': chunk})}\n\n"
                
                yield f"data: {json.dumps({'type': 'done'})}\n\n"

        except Exception as e:
            yield f"data: {json.dumps({'type': 'error', 'content': str(e)})}\n\n"

    return Response(process_stream(messages), mimetype="text/event-stream")

# --- Proactive Notification Routes ---
@app.route("/notifications")
def get_notification():
    """Poll for pending proactive notification (clears it)."""
    notif = proactive_engine.get_pending()
    return {"notification": notif}

@app.route("/notifications/peek")
def peek_notification():
    """Check if notification exists without clearing."""
    return {"has_notification": proactive_engine.peek_pending()}

@app.route("/notifications/history")
def notification_history():
    """Return recent notification history."""
    return {"history": proactive_engine.get_history()}

@app.route("/notifications/test_idle")
def test_idle_trigger():
    """Force-fire an idle loneliness notification (bypasses cooldowns)."""
    import random
    from proactive_engine import IDLE_MESSAGES
    mood, message = random.choice(IDLE_MESSAGES)
    proactive_engine._dispatch(mood, message, "test:idle")
    return {"status": "fired", "mood": mood, "message": message}

@app.route("/notifications/settings", methods=["GET", "POST"])
def notification_settings():
    """Get or update proactive notification settings."""
    if request.method == "POST":
        data = request.json or {}
        if "enabled" in data:
            proactive_engine.set_enabled(bool(data["enabled"]))
        if "quiet_start" in data and "quiet_end" in data:
            proactive_engine.set_quiet_hours(int(data["quiet_start"]), int(data["quiet_end"]))
        return {"status": "updated", **proactive_engine.get_settings()}
    return proactive_engine.get_settings()

@app.route("/audio_data")
def get_audio_data():
    """Returns current audio peak for visualizers."""
    return {"peak": audio_peak}

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)