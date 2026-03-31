# Sprite Animator - With Animation Support

import utime
import framebuf
import machine
from machine import Pin, I2C, PWM, RTC
import _thread

# Display constants
WIDTH = 128
HEIGHT = 64

# Import sprite data from separate files
from sprites_idle import IDLE_1, IDLE_2

# Optional: import other animations when ready
try:
    from sprites_alert import ALERT_1, ALERT_2, ALERT_3
    HAS_ALERT = True
except ImportError:
    HAS_ALERT = False
    ALERT_1 = ALERT_2 = ALERT_3 = None

try:
    from sprites_sleeping import SLEEP_1, SLEEP_2, SLEEP_3, SLEEP_4
    HAS_SLEEP = True
except ImportError:
    HAS_SLEEP = False
    SLEEP_1 = SLEEP_2 = SLEEP_3 = SLEEP_4 = None

# Animation definitions - only include available animations
ANIMATIONS = {}
if HAS_ALERT:
    ANIMATIONS['alert'] = ([ALERT_1, ALERT_2, ALERT_3], 200)
if HAS_SLEEP:
    ANIMATIONS['sleeping'] = ([SLEEP_1, SLEEP_2, SLEEP_3, SLEEP_4], 800)

# Always include idle animation
ANIMATIONS['idle'] = ([IDLE_1, IDLE_2], 500)


class SpriteAnimator:
    def __init__(self, display):
        self.display = display
        self.current_anim = None
        self.frames = []
        self.frame_speed = 500
        self.frame_index = 0
        self.last_update = 0
        self.x_offset = 2
        self.y_offset = 0
    
    def load_animation(self, name):
        """Load animation by name"""
        if name not in ANIMATIONS:
            print(f"Animation '{name}' not found")
            return False
        
        self.frames, self.frame_speed = ANIMATIONS[name]
        self.current_anim = name
        self.frame_index = 0
        self.last_update = utime.ticks_ms()
        return True
    
    def _draw_frame(self):
        """Draw current frame to display"""
        global _i2c_lock
        
        if not self.frames or not self.display:
            return
        
        # Wait for I2C lock to be available
        while _i2c_lock:
            utime.sleep_ms(1)
        
        # Acquire lock
        _i2c_lock = True
        
        try:
            # Create framebuffer from raw sprite data
            sprite_data = self.frames[self.frame_index]
            sprite_fb = framebuf.FrameBuffer(sprite_data, 128, 64, framebuf.MONO_HLSB)
            
            # Fill and blit
            self.display.fill(0)
            self.display.blit(sprite_fb, self.x_offset, self.y_offset)
            self.display.show()
        finally:
            # Always release lock
            _i2c_lock = False
    
    def update(self):
        """Call this in main loop - returns True if frame changed"""
        if not self.frames:
            return False
        
        now = utime.ticks_ms()
        if utime.ticks_diff(now, self.last_update) >= self.frame_speed:
            self.frame_index = (self.frame_index + 1) % len(self.frames)
            self.last_update = now
            self._draw_frame()
            return True
        
        return False
    
    def show(self):
        """Force show current frame"""
        self._draw_frame()
    
    def clear(self):
        """Clear display"""
        global _i2c_lock
        
        if self.display:
            # Wait for I2C lock to be available
            while _i2c_lock:
                utime.sleep_ms(1)
            
            # Acquire lock
            _i2c_lock = True
            
            try:
                self.display.fill(0)
                self.display.show()
            finally:
                # Always release lock
                _i2c_lock = False
    
    def sleep(self, enable=True):
        """Enable/disable sleep mode"""
        if self.display:
            self.display.sleep(enable)


# ===== GLOBAL STATE =====

# Core instances
_display = None
_animator = None
_rtc = None
_buzzer = None

# Dual core state
_core1_running = False
_sprite_paused = False
_i2c_lock = False

# Alert system state
_alert_active = False
_alert_start_time = 0
_last_press_time = 0

# Time display state
_showing_time = False
_time_display_start = 0
_last_time_redraw = 0

# Constants
DEBOUNCE_MS = 300
ALERT_DURATION_MS = 2500
TIME_DISPLAY_MS = 5000
BUZZER_PIN = 12

# Alert tune
ALERT_TUNE = [
    (523, 150),   # C5
    (659, 150),   # E5
    (784, 150),   # G5
    (1047, 300),  # C6
]

# Tune playback state
_tune_index = 0
_tune_start_time = 0
_tune_playing = False


# ===== DUAL CORE FUNCTIONS =====

def _core1_sprite_loop():
    """Core 1: Runs sprite animation continuously"""
    global _core1_running, _sprite_paused, _i2c_lock
    
    while _core1_running:
        try:
            if not _sprite_paused and _animator and _animator.frames and not _i2c_lock:
                _animator.update()
        except OSError as e:
            print(f"I2C error in sprite loop: {e}")
        except Exception as e:
            print(f"Error in sprite loop: {e}")
        
        utime.sleep_ms(10)


def start_dual_core():
    """Start dual-core mode: Core 1 handles sprites, Core 0 handles main code"""
    global _core1_running
    _core1_running = True
    _thread.start_new_thread(_core1_sprite_loop, ())
    print("Dual core started - Core 1: sprites, Core 0: your code")


def stop_dual_core():
    """Stop the second core's sprite loop"""
    global _core1_running
    _core1_running = False
    utime.sleep_ms(50)


# ===== INITIALIZATION =====

def init(i2c=None, scl_pin=5, sda_pin=4, res_pin=16, addr=0x3c):
    """Initialize the sprite system"""
    global _display, _animator
    
    if i2c is None:
        i2c = I2C(scl=Pin(scl_pin), sda=Pin(sda_pin), freq=400000)
    
    import sh1106
    _display = sh1106.SH1106_I2C(WIDTH, HEIGHT, i2c, Pin(res_pin), addr, rotate=180)
    _display.sleep(False)
    
    _animator = SpriteAnimator(_display)
    init_rtc()


def init_rtc():
    """Initialize internal RTC"""
    global _rtc
    if _rtc is None:
        _rtc = RTC()
        
        # Check if RTC has default time (2021 is Pico default)
        year, month, day, _, hour, minute, second, _ = _rtc.datetime()
        if year == 2021:
            print("RTC has default time - setting to current time...")
            # Set to a reasonable current time (you should adjust this)
            _rtc.datetime((2026, 3, 24, 1, 15, 30, 0, 0))  # Monday 3:30 PM
            print(f"RTC set to: 2026-03-24 15:30:00")
        else:
            print(f"RTC already has time: {year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}")
    
    return _rtc


# ===== ANIMATION CONTROL =====

def play_animation(name):
    """Switch to named animation"""
    if _animator:
        _animator.load_animation(name)
        _animator.show()


def idle():
    """Switch to idle animation"""
    global _alert_active
    if _animator:
        _animator.load_animation('idle')
        _animator.show()
        _alert_active = False


def update():
    """Call this in your main loop"""
    if _animator:
        _animator.update()


# ===== ALERT SYSTEM =====

def trigger_alert():
    """Trigger alert animation for configured duration"""
    global _alert_active, _alert_start_time, _last_press_time
    
    if not _animator:
        return False
    
    now = utime.ticks_ms()
    
    # Check debounce
    if utime.ticks_diff(now, _last_press_time) < DEBOUNCE_MS:
        return False
    
    # Don't trigger if already active or showing time
    if _showing_time or _alert_active:
        return False
    
    _last_press_time = now
    _alert_start_time = now
    _alert_active = True
    _animator.load_animation('alert')
    _animator.show()
    return True


def update_alert():
    """Call this in your main loop to check if alert timer expired"""
    global _alert_active, _showing_time, _time_display_start, _last_time_redraw, _sprite_paused
    
    update_tune()
    
    # Handle time display phase
    if _showing_time and _animator:
        if utime.ticks_diff(utime.ticks_ms(), _time_display_start) >= TIME_DISPLAY_MS:
            _showing_time = False
            if _alert_active:
                _alert_active = False
            _sprite_paused = False
            idle()
            return True
        else:
            # Throttle time display refresh
            now = utime.ticks_ms()
            if utime.ticks_diff(now, _last_time_redraw) >= 500:
                _last_time_redraw = now
                _draw_time_display()
    
    # Handle alert phase
    if _alert_active and _animator:
        if utime.ticks_diff(utime.ticks_ms(), _alert_start_time) >= ALERT_DURATION_MS:
            stop_tune()
            show_time_display()
            _alert_active = False
    
    return False


def alert_with_sound():
    """Trigger alert with sound"""
    if trigger_alert():
        play_alert_tune()


# ===== BUZZER FUNCTIONS =====

def init_buzzer(pin=12):
    """Initialize buzzer on specified pin using hardware PWM"""
    global _buzzer, BUZZER_PIN
    BUZZER_PIN = pin
    _buzzer = PWM(Pin(pin))
    _buzzer.duty_u16(0)
    return _buzzer


def buzzer_tone(frequency_hz, duration_ms=None):
    """Play a tone on the buzzer"""
    if _buzzer is None:
        init_buzzer()
    
    _buzzer.freq(frequency_hz)
    _buzzer.duty_u16(32768)
    
    if duration_ms is not None:
        utime.sleep_ms(duration_ms)
        _buzzer.duty_u16(0)


def buzzer_beep(beeps=3, freq=1000, on_ms=100, off_ms=100):
    """Play beep pattern"""
    if _buzzer is None:
        init_buzzer()
    
    for i in range(beeps):
        _buzzer.freq(freq)
        _buzzer.duty_u16(32768)
        utime.sleep_ms(on_ms)
        _buzzer.duty_u16(0)
        if i < beeps - 1:
            utime.sleep_ms(off_ms)


def stop_buzzer():
    """Stop the buzzer immediately"""
    if _buzzer:
        _buzzer.duty_u16(0)


# ===== TUNE PLAYBACK =====

def play_alert_tune():
    """Play the alert tune (non-blocking via polling)"""
    global _tune_index, _tune_start_time, _tune_playing
    
    if _buzzer is None:
        init_buzzer()
    
    _tune_index = 0
    _tune_start_time = utime.ticks_ms()
    _tune_playing = True
    
    # Play first note immediately
    if _tune_index < len(ALERT_TUNE):
        freq, _ = ALERT_TUNE[_tune_index]
        _buzzer.freq(freq)
        _buzzer.duty_u16(32768)


def update_tune():
    """Call this in your main loop to advance the tune"""
    global _tune_index, _tune_start_time, _tune_playing
    
    if not _tune_playing or _buzzer is None:
        return False
    
    if _tune_index >= len(ALERT_TUNE):
        stop_buzzer()
        _tune_playing = False
        return True
    
    # Check if current note duration has passed
    _, duration = ALERT_TUNE[_tune_index]
    elapsed = utime.ticks_diff(utime.ticks_ms(), _tune_start_time)
    
    if elapsed >= duration:
        _tune_index += 1
        
        if _tune_index >= len(ALERT_TUNE):
            stop_buzzer()
            _tune_playing = False
            return True
        else:
            _tune_start_time = utime.ticks_ms()
            freq, _ = ALERT_TUNE[_tune_index]
            _buzzer.freq(freq)
            _buzzer.duty_u16(32768)
    
    return False


def stop_tune():
    """Stop the tune immediately"""
    global _tune_playing
    _tune_playing = False
    stop_buzzer()


# ===== TIME/DISPLAY FUNCTIONS =====

def set_rtc_datetime(year, month, day, hour, minute, second=0):
    """Set internal RTC datetime"""
    global _rtc
    if _rtc is None:
        init_rtc()
    _rtc.datetime((year, month, day, 0, hour, minute, second, 0))
    print(f"RTC set to: {year:04d}-{month:02d}-{day:02d} {hour:02d}:{minute:02d}:{second:02d}")


def get_time_string():
    """Get current time as string (12-hour format with AM/PM)"""
    if _rtc is None:
        init_rtc()
    year, month, day, _, hour, minute, second, _ = _rtc.datetime()
    
    # Convert to 12-hour format
    if hour == 0:
        hour_12 = 12
        am_pm = "AM"
    elif hour < 12:
        hour_12 = hour
        am_pm = "AM"
    elif hour == 12:
        hour_12 = 12
        am_pm = "PM"
    else:
        hour_12 = hour - 12
        am_pm = "PM"
    
    return "{:02d}:{:02d}:{:02d} {}".format(hour_12, minute, second, am_pm)


def get_date_string():
    """Get current date as string (YYYY-MM-DD)"""
    if _rtc is None:
        init_rtc()
    year, month, day, _, _, _, _, _ = _rtc.datetime()
    return "{:04d}-{:02d}-{:02d}".format(year, month, day)


def show_time_display():
    """Start showing time/date on display for 5 seconds"""
    global _showing_time, _time_display_start, _sprite_paused
    
    if _display is None:
        return
    
    _sprite_paused = True
    _showing_time = True
    _time_display_start = utime.ticks_ms()
    _draw_time_display()


def _draw_time_display():
    """Draw time and date on OLED"""
    global _i2c_lock
    
    if _display is None:
        return
    
    # Wait for I2C lock to be available
    while _i2c_lock:
        utime.sleep_ms(1)
    
    # Acquire lock
    _i2c_lock = True
    
    try:
        _display.fill(0)
        
        # Draw time (larger)
        time_str = get_time_string()
        _display.text(time_str, 20, 10, 1)
        
        # Draw date
        date_str = get_date_string()
        _display.text(date_str, 18, 30, 1)
        
        _display.show()
    finally:
        # Always release lock
        _i2c_lock = False


# ===== DISPLAY UTILITIES =====

def fill(color):
    """Fill display with color"""
    if _display:
        _display.fill(color)


def text(string, x, y, color=1):
    """Draw text"""
    if _display:
        _display.text(string, x, y, color)


def show():
    """Show display"""
    if _display:
        _display.show()


def cleanup():
    """Clear display and put to sleep"""
    global _i2c_lock
    
    # Wait for any ongoing I2C operations to complete
    while _i2c_lock:
        utime.sleep_ms(1)
    
    if _animator:
        _animator.clear()
        _animator.sleep(True)


# ===== EXAMPLE USAGE =====

def main():
    """Example: Dual core with button + sensor events"""
    # Initialize (runs on Core 0)
    init()
    init_buzzer(12)
    
    # Set RTC time (example - set once)
    # set_rtc_datetime(2026, 3, 24, 15, 1, 0)  # Year, Month, Day, Hour, Minute, Second
    
    # Start dual core mode - sprites run on Core 1 automatically
    start_dual_core()
    
    # Set up button on GP13 (Core 0)
    button = Pin(13, Pin.IN, Pin.PULL_DOWN)
    
    # Start with idle (Core 0)
    idle()
    
    try:
        # Core 0 main loop - handles button, sensors, buzzer
        while True:
            # Check alert timer (auto-returns to idle after 3s, stops tune)
            if update_alert():
                print("Alert ended, back to idle")
            
            # Button press triggers alert (Core 0)
            if button.value() and not _alert_active:
                if trigger_alert():
                    play_alert_tune()
                while button.value():
                    update_alert()  # Keep checking timer while waiting
            
            # ... other Core 0 tasks: sensors, communication, etc.
            
    except KeyboardInterrupt:
        print("\nStopping...")
    
    finally:
        stop_dual_core()
        stop_buzzer()
        cleanup()


# ===== BACKWARD COMPATIBILITY =====
# Legacy function names for compatibility
Idle = idle
Sprite = play_animation


if __name__ == "__main__":
    main()
