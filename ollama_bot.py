import requests
import json
import re
import sys
import time
import threading

# --- CONFIGURATION ---
ESP32_IP = "10.42.0.145"  
OLLAMA_URL = "http://localhost:11434/api/chat"

# CHANGED: Use the conversational instruct model instead of the coder model.
# Make sure you run `ollama run qwen2.5:3b` in your terminal to download it!
OLLAMA_MODEL = "qwen2.5:3b" 

# --- IDLE STATE MACHINE TIMINGS (seconds) ---
HOLD_EMOTION_SECS = 30   # Hold last chat emotion before reverting
NORMAL_AFTER_SECS = 30   # Settled idle → NORMAL
BORED_AFTER_SECS  = 120  # Long idle → BORED (re-uses SUSPICIOUS visually)
SLEEP_AFTER_SECS  = 300  # Very long idle → SLEEP

last_interaction = time.time()

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
"""
# --- WEATHER ---
def get_weather():
    try:
        # Get location via IP
        loc_res = requests.get("http://ip-api.com/json/", timeout=2).json()
        lat, lon, city = loc_res['lat'], loc_res['lon'], loc_res['city']
        
        # Get weather via Open-Meteo
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
    except Exception as e:
        pass 

def _set_esp32_mode(mode_id):
    """Set the ESP32 mode directly."""
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return
    try:
        requests.get(f"http://{ESP32_IP}/mode?set={mode_id}", timeout=1)
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
            set_esp32_mood("SLEEP")
        elif state == "BORED":
            # Rotate through BORED eyes and GAME mode every minute
            if int(elapsed / 60) % 2 == 0:
                set_esp32_mood("BORED")
            else:
                _set_esp32_mode(15)
        elif state == "NORMAL":
            set_esp32_mood("NORMAL")

        prev_state = state

def chat_loop():
    print("🤖 Emotional Ollama Bot Initialized!")
    print(f"Current IP: {ESP32_IP}")
    print(f"Using Model: {OLLAMA_MODEL}")
    print("Type 'exit' or 'quit' to leave.\n")
    
    weather_info, initial_mood = get_weather()
    if weather_info:
        print(f"🌍 Weather Update: {weather_info}")
        set_esp32_mood(initial_mood)
    else:
        set_esp32_mood("NORMAL")

    # Dynamic System Prompt with Weather
    current_prompt = SYSTEM_PROMPT
    if weather_info:
        current_prompt += f"\n\nCURRENT CONTEXT: The weather outside is {weather_info}. If relevant, you can react to it!"

    messages = [{"role": "system", "content": current_prompt}]

    # Start idle state machine background thread
    threading.Thread(target=idle_loop, daemon=True).start()

    while True:
        try:
            user_input = input("\nYou: ")
            if user_input.lower() in ['exit', 'quit']:
                print("Sleeping! zzz...")
                set_esp32_mood("SLEEP")
                break
            
            messages.append({"role": "user", "content": user_input})
            global last_interaction
            last_interaction = time.time()  # Reset idle timer
            print("Bot: ", end="", flush=True)
            
            payload = {
                "model": OLLAMA_MODEL,
                "messages": messages,
                "stream": True 
            }
            
            try:
                response = requests.post(OLLAMA_URL, json=payload, stream=True)
                if response.status_code != 200:
                    print(f"\n[Error: Ollama API returned {response.status_code}]")
                    continue
            except requests.exceptions.ConnectionError:
                print("\n[Error: Could not connect to Ollama.]")
                continue
                
            full_reply = ""
            tag_buffer = ""
            tag_found = False
            
            for line in response.iter_lines():
                if line:
                    data = json.loads(line)
                    chunk = data["message"]["content"]
                    full_reply += chunk
                    
                    # CHANGED: More robust parsing logic to prevent silent failures
                    if not tag_found:
                        tag_buffer += chunk
                        
                        if "]" in tag_buffer:
                            match = re.search(r'\[(.*?)\]', tag_buffer)
                            if match:
                                emotion = match.group(1)
                                set_esp32_mood(emotion) 
                                text_after_tag = tag_buffer[match.end():]
                                print(text_after_tag.lstrip(), end="", flush=True)
                            else:
                                print(tag_buffer, end="", flush=True)
                            tag_found = True
                            
                        # Fallback: If the model generated 20 chars without a bracket, it broke the rules. 
                        # Stop waiting and just print it so you can see what it's saying.
                        elif len(tag_buffer) > 20 and "[" not in tag_buffer:
                            print(tag_buffer, end="", flush=True)
                            tag_found = True 
                    else:
                        print(chunk, end="", flush=True) 
                        
            print() 
            messages.append({"role": "assistant", "content": full_reply})
            
            # Removed hardcoded NORMAL reset; let idle_loop handle it after 30s
            pass
            
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"\n[Error: {e}]")

if __name__ == "__main__":
    chat_loop()