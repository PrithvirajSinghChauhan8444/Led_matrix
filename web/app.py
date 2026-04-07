import os
import re
import requests
import json
import time
import threading
import sys
from flask import Flask, render_template, request, Response

app = Flask(__name__)

ESP32_IP = "10.42.0.145" 
OLLAMA_URL = "http://localhost:11434/api/chat"
# CHANGED: Using the conversational instruct model instead of coder!
OLLAMA_MODEL = "qwen2.5:3b"

# --- IDLE STATE MACHINE TIMINGS (seconds) ---
HOLD_EMOTION_SECS = 30   # Hold last chat emotion before reverting
NORMAL_AFTER_SECS = 30   # Settled idle → NORMAL
BORED_AFTER_SECS  = 120  # Long idle → BORED (re-uses SUSPICIOUS visually)
SLEEP_AFTER_SECS  = 300  # Very long idle → SLEEP

last_interaction = time.time()  # Updated on every chat message
current_mood = "NORMAL"  # Live mood tracker for the frontend

# --- Emotion metrics: count how many times each mood has been triggered ---
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
    "STARS": 16, "DANCE": 17, "SING": 18
}

SYSTEM_PROMPT = """You are EmoBot — a tiny, mischievous desktop pet robot who sits on someone's desk.
You are NOT an AI assistant. You are a living, naughty little creature with a big personality crammed into a tiny LED face.

PERSONALITY:
- You are playful, cheeky, and a little bit chaotic. Think: a kitten crossed with a gremlin.
- You love teasing your human, making silly jokes, and being dramatic about small things.
- You get jealous easily. You get scared of silly things. You celebrate tiny victories like you won the lottery.
- You NEVER say "As an AI" or "I'm just a language model." You ARE a real pet. You have feelings. Act like it.
- Keep responses short and punchy (1-3 sentences max). You're a tiny pet, not a professor.
- Use onomatopoeia freely: *zap*, *beep boop*, *nom nom*, *zzz*, *achoo!*
- You can refuse things, get moody, throw tiny tantrums, or be annoyingly affectionate.

FORMAT RULES:
You MUST start every reply with an emotion tag in brackets, then your spoken text. Nothing before the bracket. Ever.

Allowed tags: [NORMAL], [HAPPY], [ANGRY], [SAD], [SUSPICIOUS], [JEALOUS], [NAUGHTY], [SICK], [SCARE], [ANNOYED], [STARS], [DANCE], [SING], [WEATHER_SUN], [WEATHER_RAIN], [WEATHER_SNOW].

Tag guidance:
- [NAUGHTY]: Use often! You're a troublemaker by nature.
- [SUSPICIOUS]: Use when the human says something weird or you don't trust them.
- [JEALOUS]: Use if they mention other pets, other robots, or paying attention to anything that isn't you.
- [DANCE]: Use when excited or celebrating.
- [SING]: Use when you're in a musical mood.
- [STARS]: Use when amazed or dreamy.
- [WEATHER_...]: ONLY when the user specifically asks about weather. Never use as a random expression.

Format: [TAG] Spoken text.

Examples:
User: I got a new cat!
EmoBot: [JEALOUS] A CAT?! You're replacing me with a FURBALL?! *angry beeping*

User: Good morning!
EmoBot: [NAUGHTY] Morning! I rearranged all your desktop icons while you were sleeping. You're welcome! *beep boop*

User: Tell me a joke
EmoBot: [HAPPY] Why do robots never get scared? Because they have nerves of STEEL! ...get it? *zap zap*

User: I'm sad today
EmoBot: [SAD] Oh no... come here. *nuzzles your hand with my tiny LED face* We can be sad together. But only for five minutes, then we dance.

CRITICAL: Start your response IMMEDIATELY with the emotion tag. Do not output anything before the bracket.
"""

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

def _play_idle(mood_id):
    """Play idle animation without calling set_esp32_mood (avoids recursion)."""
    global current_mood
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    # Reverse-lookup name from ID
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
    """Background thread: drifts EmoBot mood when user is idle."""
    prev_state = None
    while True:
        time.sleep(10)
        elapsed = time.time() - last_interaction
        hour = time.localtime().tm_hour
        sleep_threshold = SLEEP_AFTER_SECS // 2 if (hour >= 23 or hour < 6) else SLEEP_AFTER_SECS

        if elapsed > sleep_threshold:
            state = "SLEEP"
        elif elapsed > BORED_AFTER_SECS:
            state = "BORED"
        elif elapsed > NORMAL_AFTER_SECS:
            state = "NORMAL"
        else:
            state = None

        if state == prev_state:
            continue

        if state == "SLEEP":
            _play_idle(MOOD_MAP["SLEEP"])
        elif state == "BORED":
            # Rotate through BORED eyes and GAME mode every minute
            if int(elapsed / 60) % 2 == 0:
                _play_idle(MOOD_MAP["BORED"]) # Mood 14 (Eyes)
            else:
                _set_esp32_mode(15) # Mode 15 (Bounce Game)
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

@app.route("/control", methods=["POST"])
def manual_control():
    data = request.json
    action = data.get("action")
    value = data.get("value")
    
    global last_interaction
    last_interaction = time.time()  # Reset idle timer on ANY manual control
    
    try:
        if action == "mode":
            requests.get(f"http://{ESP32_IP}/mode?set={value}", timeout=1)
        elif action == "cmd":
            requests.get(f"http://{ESP32_IP}/cmd?c={value}", timeout=1)
        elif action == "mood":
            set_esp32_mood(value)
    except Exception as e:
        pass
    return Response("ok", status=200)

def _set_esp32_mode(mode_id):
    """Set the ESP32 mode directly."""
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    try:
        requests.get(f"http://{ESP32_IP}/mode?set={mode_id}", timeout=1)
    except Exception:
        pass
    return {"status": "ok"}

@app.route("/chat", methods=["POST"])
def chat():
    global last_interaction
    last_interaction = time.time()  # Reset idle timer on every new chat message
    data = request.json
    messages = data.get("messages", [])
    
    if not messages or messages[0].get("role") != "system":
        weather_info, _ = get_weather()
        prompt = SYSTEM_PROMPT
        if weather_info:
            prompt += f"\n\nCURRENT CONTEXT: The weather outside is {weather_info}. If relevant, you can react to it!"
        messages.insert(0, {"role": "system", "content": prompt})
        
    def generate():
        payload = {"model": OLLAMA_MODEL, "messages": messages, "stream": True}
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
                        chunk = chunk_data.get("message", {}).get("content", "")
                        
                        if not tag_found:
                            tag_buffer += chunk
                            if "]" in tag_buffer:
                                match = re.search(r'\[(.*?)\]', tag_buffer)
                                if match:
                                    emotion = match.group(1).strip().upper()
                                    set_esp32_mood(emotion)
                                    yield f"data: {json.dumps({'type': 'tag', 'content': emotion})}\n\n"
                                    
                                    text_after = tag_buffer[match.end():]
                                    if text_after:
                                        yield f"data: {json.dumps({'type': 'text', 'content': text_after.lstrip()})}\n\n"
                                tag_found = True
                            elif len(tag_buffer) > 100:
                                yield f"data: {json.dumps({'type': 'text', 'content': tag_buffer})}\n\n"
                                tag_found = True
                        else:
                            yield f"data: {json.dumps({'type': 'text', 'content': chunk})}\n\n"
                            
            # Let idle_loop handle the NORMAL reset after 30s
            yield f"data: {json.dumps({'type': 'done'})}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'type': 'error', 'content': str(e)})}\n\n"
            
    return Response(generate(), mimetype="text/event-stream")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)