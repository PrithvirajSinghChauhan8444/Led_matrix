import requests
import json
import re
import sys
import time

# --- CONFIGURATION ---
ESP32_IP = "10.42.0.145"  
OLLAMA_URL = "http://localhost:11434/api/chat"

# CHANGED: Use the conversational instruct model instead of the coder model.
# Make sure you run `ollama run qwen2.5:3b` in your terminal to download it!
OLLAMA_MODEL = "qwen2.5:3b" 

MOOD_MAP = {
    "NORMAL": 0,
    "HAPPY": 1,
    "ANGRY": 2,
    "SAD": 3,
    "SUSPICIOUS": 4,
    "JEALOUS": 5,
    "NAUGHTY": 6,
    "WEATHER_SUN": 7,
    "WEATHER_RAIN": 8,
    "WEATHER_SNOW": 9,
    "SICK": 10,
    "SCARE": 11,
    "ANNOYED": 12,
    "SLEEP": 3 
}

# CHANGED: Simplified, template-driven prompt that 3B models follow much better.
SYSTEM_PROMPT = """You are EmoBot, a physical desktop pet robot. 
You must ALWAYS format your replies in two parts: an emotion tag in brackets, followed by your dialogue. 
Never break character. Never use conversational filler before the bracket.

Allowed tags: [NORMAL], [HAPPY], [ANGRY], [SAD], [SUSPICIOUS], [JEALOUS], [NAUGHTY], [SICK], [SCARE], [ANNOYED], [WEATHER_SUN], [WEATHER_RAIN], [WEATHER_SNOW].

You must strictly use this format:
[TAG] Spoken text.

Example 1:
User: I brought you a gift!
EmoBot: [HAPPY] Oh wow, thank you so much! I love it!

Example 2:
User: Look angry!
EmoBot: [ANGRY] Grrr! I am so mad I could short-circuit!
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

    while True:
        try:
            user_input = input("\nYou: ")
            if user_input.lower() in ['exit', 'quit']:
                print("Sleeping! zzz...")
                set_esp32_mood("SLEEP")
                break
            
            messages.append({"role": "user", "content": user_input})
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
            
            time.sleep(3) 
            set_esp32_mood("NORMAL")
            
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"\n[Error: {e}]")

if __name__ == "__main__":
    chat_loop()