import time
import threading
import subprocess
import config

try:
    import psutil
except ImportError:
    psutil = None


def _human_size(value):
    if value is None:
        return "N/A"
    units = ["B", "KB", "MB", "GB", "TB"]
    for unit in units:
        if value < 1024 or unit == units[-1]:
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


def _read_gpu_temp():
    try:
        result = subprocess.run(
            ["nvidia-smi", "--query-gpu=temperature.gpu", "--format=csv,noheader,nounits"],
            capture_output=True,
            text=True,
            timeout=1,
        )
        if result.returncode != 0:
            return None
        raw = result.stdout.strip().splitlines()
        if not raw:
            return None
        return float(raw[0].strip())
    except Exception:
        return None


def _get_cpu_temp():
    if not psutil:
        return None
    temps = psutil.sensors_temperatures()
    if not temps:
        return None
    for key in ("coretemp", "cpu_thermal", "cpu-thermal", "acpitz"):
        entries = temps.get(key)
        if entries:
            for entry in entries:
                if entry.current is not None and entry.current > 0:
                    return entry.current
    for entries in temps.values():
        for entry in entries:
            if entry.current is not None and entry.current > 0:
                return entry.current
    return None


def _is_network_up():
    if not psutil:
        return None
    try:
        interfaces = psutil.net_if_stats()
        return any(stat.isup for name, stat in interfaces.items() if name != "lo")
    except Exception:
        return None


class SystemMonitor:
    def __init__(self, poll_interval=10, disk_path="/"):
        self.poll_interval = poll_interval
        self.disk_path = disk_path
        self._lock = threading.Lock()
        self._snapshot = self._empty_snapshot()
        self._last_net = None
        self._thread = None
        self._stop_event = threading.Event()
        self.update()

    def _empty_snapshot(self):
        return {
            "cpu_percent": 0.0,
            "ram_percent": 0.0,
            "ram_used": 0,
            "ram_total": 0,
            "disk_percent": 0.0,
            "disk_free_percent": 0.0,
            "disk_free": 0,
            "battery_percent": None,
            "battery_plugged": None,
            "battery_secsleft": None,
            "cpu_temp_c": None,
            "gpu_temp_c": None,
            "network_up": None,
            "network_bytes_sent": 0,
            "network_bytes_recv": 0,
            "network_sent_rate": 0.0,
            "network_recv_rate": 0.0,
            "uptime_secs": 0,
            "time_of_day": _time_of_day_label(),
            "last_updated": time.time(),
            "override_mood": None,
            "override_reason": None,
        }

    def start(self):
        if self._thread is not None:
            return
        self._thread = threading.Thread(target=self._run, daemon=True)
        self._thread.start()

    def stop(self):
        self._stop_event.set()
        if self._thread is not None:
            self._thread.join(timeout=1)

    def _run(self):
        while not self._stop_event.is_set():
            self.update()
            self._stop_event.wait(self.poll_interval)

    def update(self):
        snapshot = self._empty_snapshot()
        snapshot["last_updated"] = time.time()

        if psutil is None:
            snapshot["override_reason"] = "psutil is unavailable"
            with self._lock:
                self._snapshot = snapshot
            return

        try:
            snapshot["cpu_percent"] = psutil.cpu_percent(interval=None)
            ram = psutil.virtual_memory()
            snapshot["ram_percent"] = ram.percent
            snapshot["ram_used"] = ram.used
            snapshot["ram_total"] = ram.total

            disk = psutil.disk_usage(self.disk_path)
            snapshot["disk_percent"] = disk.percent
            snapshot["disk_free"] = disk.free
            snapshot["disk_free_percent"] = 100.0 - disk.percent

            battery = psutil.sensors_battery()
            if battery is not None:
                snapshot["battery_percent"] = battery.percent
                snapshot["battery_plugged"] = battery.power_plugged
                snapshot["battery_secsleft"] = battery.secsleft

            snapshot["cpu_temp_c"] = _get_cpu_temp()
            snapshot["gpu_temp_c"] = _read_gpu_temp()
            snapshot["network_up"] = _is_network_up()

            net = psutil.net_io_counters()
            now = time.time()
            snapshot["network_bytes_sent"] = net.bytes_sent
            snapshot["network_bytes_recv"] = net.bytes_recv
            if self._last_net is not None:
                last_net, last_time = self._last_net
                interval = max(1.0, now - last_time)
                snapshot["network_sent_rate"] = (net.bytes_sent - last_net.bytes_sent) / interval
                snapshot["network_recv_rate"] = (net.bytes_recv - last_net.bytes_recv) / interval
            else:
                snapshot["network_sent_rate"] = 0.0
                snapshot["network_recv_rate"] = 0.0
            self._last_net = (net, now)

            snapshot["uptime_secs"] = max(0, time.time() - psutil.boot_time())
            snapshot["time_of_day"] = _time_of_day_label()

            override = self._calculate_override(snapshot)
            snapshot["override_mood"] = override.get("mood")
            snapshot["override_reason"] = override.get("reason")

        except Exception as exc:
            snapshot["override_reason"] = f"failed to collect system stats: {exc}"

        with self._lock:
            self._snapshot = snapshot

    def _calculate_override(self, snapshot):
        if snapshot["battery_percent"] is not None and snapshot["battery_percent"] < config.BATTERY_LOW_PERCENT and snapshot["battery_plugged"] is False:
            return {"mood": "SCARE", "reason": "battery is critically low"}
        if snapshot["cpu_percent"] >= config.CPU_WARN_PERCENT:
            return {"mood": "ANNOYED", "reason": f"CPU usage is above {config.CPU_WARN_PERCENT}%"}
        if snapshot["ram_percent"] >= config.RAM_WARN_PERCENT:
            return {"mood": "SICK", "reason": f"RAM usage is above {config.RAM_WARN_PERCENT}%"}
        if snapshot["disk_free_percent"] is not None and snapshot["disk_free_percent"] < config.DISK_WARN_PERCENT:
            return {"mood": "SUSPICIOUS", "reason": f"disk free space is below {config.DISK_WARN_PERCENT}%"}
        if snapshot["cpu_temp_c"] is not None and snapshot["cpu_temp_c"] >= config.CPU_TEMP_WARN_C:
            return {"mood": "ANGRY", "reason": f"CPU temperature is above {config.CPU_TEMP_WARN_C}°C"}
        if snapshot["gpu_temp_c"] is not None and snapshot["gpu_temp_c"] >= config.GPU_TEMP_WARN_C:
            return {"mood": "SCARE", "reason": f"GPU temperature is above {config.GPU_TEMP_WARN_C}°C"}
        if snapshot["network_up"] is False:
            return {"mood": "SAD", "reason": "network appears disconnected"}
        if snapshot["uptime_secs"] >= 12 * 3600:
            return {"mood": "BORED", "reason": "system uptime has exceeded 12 hours"}
        return {"mood": None, "reason": None}

    def get_snapshot(self):
        with self._lock:
            return dict(self._snapshot)

    def get_mood_override(self):
        with self._lock:
            return {
                "mood": self._snapshot.get("override_mood"),
                "reason": self._snapshot.get("override_reason"),
            }
