import os
import re
import requests
import json
import time
from flask import Flask, render_template, request, Response

app = Flask(__name__)

ESP32_IP = "10.42.0.145" # Extracted from original bot
OLLAMA_URL = "http://localhost:11434/api/chat"
OLLAMA_MODEL = "qwen2.5-coder:3b"

MOOD_MAP = {
    "NORMAL": 0, "HAPPY": 1, "ANGRY": 2, "SAD": 3,
    "SUSPICIOUS": 4, "JEALOUS": 5, "NAUGHTY": 6,
    "WEATHER_SUN": 7, "WEATHER_RAIN": 8, "WEATHER_SNOW": 9,
    "SICK": 10, "SCARE": 11, "ANNOYED": 12, "SLEEP": 3
}

SYSTEM_PROMPT = """You are a helpful, emotional AI assistant inside a physical robot.
Before you write ANY text in your response, you MUST output your current emotion in square brackets.
The strictly allowed emotions are: [NORMAL], [HAPPY], [ANGRY], [SAD], [SUSPICIOUS], [JEALOUS], [NAUGHTY], [SICK], [SCARE], [ANNOYED], [WEATHER_SUN], [WEATHER_RAIN], [WEATHER_SNOW].
For example:
[HAPPY] I would love to help you with that!
[SUSPICIOUS] Are you sure that is a safe thing to do!
[WEATHER_RAIN] Looks like it is pouring outside, maybe grab an umbrella!
[ANNOYED] Ugh, why do I have to do everything around here?

Do not include any other brackets. Start your response immediately with the emotion tag.
"""

def set_esp32_mood(mood_name):
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    mood_id = MOOD_MAP.get(mood_name.upper(), 0)
    try:
        requests.get(f"http://{ESP32_IP}/mood?set={mood_id}", timeout=1)
    except Exception:
        pass

@app.route("/")
def index():
    return render_template("index.html")

@app.route("/chat", methods=["POST"])
def chat():
    data = request.json
    messages = data.get("messages", [])
    
    # Ensure system prompt is the first message
    if not messages or messages[0].get("role") != "system":
        messages.insert(0, {"role": "system", "content": SYSTEM_PROMPT})
        
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
                            elif len(tag_buffer) > 25:
                                # The AI probably forgot the tag! Fallback to standard output.
                                yield f"data: {json.dumps({'type': 'text', 'content': tag_buffer})}\n\n"
                                tag_found = True
                        else:
                            yield f"data: {json.dumps({'type': 'text', 'content': chunk})}\n\n"
                            
            time.sleep(3)
            set_esp32_mood("NORMAL")
            yield f"data: {json.dumps({'type': 'done'})}\n\n"
        except Exception as e:
            yield f"data: {json.dumps({'type': 'error', 'content': str(e)})}\n\n"
            
    return Response(generate(), mimetype="text/event-stream")

if __name__ == "__main__":
    app.run(host="0.0.0.0", port=5000, debug=True)
