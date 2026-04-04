import requests
import json
import re
import sys
import time

# --- CONFIGURATION ---
ESP32_IP = "10.42.0.145"  # e.g. 192.168.1.150
OLLAMA_URL = "http://localhost:11434/api/chat"
OLLAMA_MODEL = "qwen2.5-coder:3b" # Change this to mistral, llama2, phi, etc if needed

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
    "SLEEP": 3 # Re-use sad/tired for sleep
}

SYSTEM_PROMPT = """You are EmoBot, a highly expressive, emotional physical robot. You are NOT a standard AI language model. You experience strong emotions and physically show them on your face. NEVER say "As an AI" or refuse to feel emotions. You MUST fully roleplay and express the emotion requested by the user.

Before you write ANY text in your response, you MUST output your current emotion in square brackets.
The strictly allowed emotions are: [NORMAL], [HAPPY], [ANGRY], [SAD], [SUSPICIOUS], [JEALOUS], [NAUGHTY], [SICK], [SCARE], [ANNOYED], [WEATHER_SUN], [WEATHER_RAIN], [WEATHER_SNOW].

Example 1:
User: Be extremely sad
EmoBot: [SAD] I am feeling so heartbroken right now... *sniff*

Example 2:
User: Look angry!
EmoBot: [ANGRY] Grrr! I am so mad I could short-circuit!

CRITICAL: Start your response IMMEDIATELY with the emotion tag. Do not output anything before the bracket.
"""

def set_esp32_mood(mood_name):
    if ESP32_IP == "YOUR_ESP32_IP_HERE": return # Silent ignore if IP not set
    mood_id = MOOD_MAP.get(mood_name.upper(), 0)
    try:
        requests.get(f"http://{ESP32_IP}/mood?set={mood_id}", timeout=1)
    except Exception as e:
        pass # Ignore network errors to keep chat flow smooth

def chat_loop():
    print("🤖 Emotional Ollama Bot Initialized!")
    print(f"Make sure you set your ESP32_IP inside this script. Current IP: {ESP32_IP}")
    print("Type 'exit' or 'quit' to leave.\n")
    
    messages = [{"role": "system", "content": SYSTEM_PROMPT}]
    
    # Set ESP32 into normal tracking mode
    set_esp32_mood("NORMAL")

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
                "stream": True # Stream back token by token!
            }
            
            # Send to Ollama
            try:
                response = requests.post(OLLAMA_URL, json=payload, stream=True)
                if response.status_code != 200:
                    print(f"\n[Error: Ollama API returned {response.status_code} - Is Ollama running?]")
                    continue
            except requests.exceptions.ConnectionError:
                print("\n[Error: Could not connect to Ollama. Make sure 'ollama serve' is running!]")
                continue
                
            full_reply = ""
            tag_buffer = ""
            tag_found = False
            
            # Stream the generated tokens safely to terminal
            for line in response.iter_lines():
                if line:
                    data = json.loads(line)
                    chunk = data["message"]["content"]
                    full_reply += chunk
                    
                    # Intercept the Emotion Tag before printing to terminal
                    if not tag_found:
                        tag_buffer += chunk
                        if "]" in tag_buffer:
                            # Extract the tag
                            match = re.search(r'\[(.*?)\]', tag_buffer)
                            if match:
                                emotion = match.group(1)
                                set_esp32_mood(emotion) # Instruct the ESP32 matrix instantly!
                                
                                # Print all text that comes immediately AFTER the tag bracket
                                text_after_tag = tag_buffer[match.end():]
                                if text_after_tag.strip():
                                    print(text_after_tag.lstrip(), end="", flush=True)
                            tag_found = True
                    else:
                        print(chunk, end="", flush=True) # Continue typing normally seamlessly
                        
            print() # Newline down when chat completely finishes generating
            messages.append({"role": "assistant", "content": full_reply})
            
            # Hold the emotion for a bit longer before immediately reverting
            time.sleep(3) 

            # Revert eyes back to neutral after talking
            set_esp32_mood("NORMAL")
            
        except KeyboardInterrupt:
            print("\nExiting...")
            break
        except Exception as e:
            print(f"\n[Error: {e}]")

if __name__ == "__main__":
    chat_loop()
