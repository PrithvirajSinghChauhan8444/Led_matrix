import subprocess
import time

def get_current_volume_peak():
    try:
        # Get peak volume from pactl
        # This is a bit hacky but works on many Linux systems with PulseAudio/Pipewire
        cmd = "pactl list sinks"
        output = subprocess.check_output(cmd, shell=True, stderr=subprocess.STDOUT).decode()
        
        # Look for the active sink (marked with *)
        # Note: This might be tricky. Alternatively, use 'pactl list sink-inputs' 
        # to see if anything is actually playing.
        
        if "Playback" in output or "Running" in output:
            return 100 # Placeholder for "music is playing"
            
    except Exception:
        pass
    return 0

if __name__ == "__main__":
    while True:
        print(f"Volume check: {get_current_volume_peak()}")
        time.sleep(1)
