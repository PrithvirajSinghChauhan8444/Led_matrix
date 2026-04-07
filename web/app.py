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

MOOD_MAP = {
    "NORMAL": 0, "HAPPY": 1, "ANGRY": 2, "SAD": 3,
    "SUSPICIOUS": 4, "JEALOUS": 5, "NAUGHTY": 6,
    "WEATHER_SUN": 7, "WEATHER_RAIN": 8, "WEATHER_SNOW": 9,
    "SICK": 10, "SCARE": 11, "ANNOYED": 12,
    "SLEEP": 13, "BORED": 14, "GAME": 15,
    "STARS": 16, "DANCE": 17, "SING": 18
}

SYSTEM_PROMPT = """You are EmoBot, a physical desktop pet robot. 
You must ALWAYS format your replies in two parts: an emotion tag in brackets, followed by your dialogue. 
Never break character. Never use conversational filler before the bracket.

Allowed tags: [NORMAL], [HAPPY], [ANGRY], [SAD], [SUSPICIOUS], [JEALOUS], [NAUGHTY], [SICK], [SCARE], [ANNOYED], [STARS], [DANCE], [SING], [WEATHER_SUN], [WEATHER_RAIN], [WEATHER_SNOW].

Emotion Context:
- [SICK]: Use if you feel unwell or sneeze.
- [ANNOYED]: Use if bothered (eye-roll).
- [SCARE]: Use for surprises.
- [STARS]: Use if you are amazed, dreaming, or looking at the night sky.
- [DANCE]: Use if you are excited, listening to music, or celebrating.
- [SING]: Use if you are happy and musical, or performing a song.
- [WEATHER_...]: ONLY use these tags if the user specifically asks about the weather. Do not use them as general facial expressions.

You must strictly use this format:
[TAG] Spoken text.

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
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    mood_id = MOOD_MAP.get(mood_name.upper(), 0)
    try:
        requests.get(f"http://{ESP32_IP}/mood?set={mood_id}", timeout=1)
    except Exception:
        pass

def _play_idle(mood_id):
    """Play idle animation without calling set_esp32_mood (avoids recursion)."""
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
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