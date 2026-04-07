#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include "DemiHandler.h"
#include "menu.h"
#include "sprite_idle.h"
#include "sprite_alert.h"
#include <esp_task_wdt.h>
#include "WiFiHandler.h"

// NTP constants
static const long gmtOffset_sec = 28800;
static const int daylightOffset_sec = 0;

// Forward declaration for tasks
void mainLoopTask(void* param);

// Pins as requested
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define RGB_LED_PIN 48 // Onboard LED for status

// Touch pins
#define TOUCH_PIN_CENTER 3
#define TOUCH_PIN_UP 4
#define TOUCH_PIN_DOWN 5
#define TOUCH_PIN_LEFT 7
#define TOUCH_PIN_RIGHT 6
#define TOUCH_THRESHOLD 50000  // Threshold for touch detection (S3 logic: higher value = touch)

// Buzzer pin
#define BUZZER_PIN 40

// Note frequencies
#define NOTE_E5 659
#define NOTE_C5 523
#define NOTE_CS5 554  // C#5
#define NOTE_C2 65   // Low bass C

// Buzzer functions using LEDC (ESP32 PWM)
void beep(uint16_t frequency, uint16_t durationMs) {
    if (frequency == 0) {
        ledcWrite(0, 0);
        return;
    }
    
    
    // Determine appropriate duty resolution based on frequency
    // Lower frequencies need higher resolution to work properly
    uint8_t dutyResolution;
    if (frequency < 100) {
        dutyResolution = 12;  // Very low freq (C2 = 65Hz)
    } else if (frequency < 200) {
        dutyResolution = 10;  // Low freq (C2-C3)
    } else if (frequency < 400) {
        dutyResolution = 8;   // Mid freq (C4-C5)
    } else {
        dutyResolution = 8;   // High freq (C5+)
    }
    
    // Setup LEDC channel 0 with calculated resolution
    ledcSetup(0, frequency, dutyResolution);
    ledcAttachPin(BUZZER_PIN, 0);
    
    // Write 50% duty cycle (half of max value for the resolution)
    uint16_t halfDuty = (1 << dutyResolution) / 2;
    ledcWrite(0, halfDuty);
    delay(durationMs);
    ledcWrite(0, 0);  // Stop
}

void beepE5(uint16_t durationMs = 50) {
    beep(NOTE_E5, durationMs);
}

void beepC5(uint16_t durationMs = 50) {
    beep(NOTE_C5, durationMs);
}

void beepCSharp(uint16_t durationMs = 50) {
    beep(NOTE_CS5, durationMs);
}

void beepLowC(uint16_t durationMs = 100) {
    beep(NOTE_C2, durationMs);
}

// Display margins
#define MARGIN 5  // 5 pixel margin from edges

// System state (what mode the device is in)
// enum SystemState {
//     STATE_IDLE,     // Sprite animation running
//     STATE_MENU,     // Menu system active
//     STATE_ALERT     // Alert animation playing
// };

// enum AnimationState {
//     ANIM_IDLE,
//     ANIM_ALERT
// };

// Ensure the correct enums from DemiHandler.h are used throughout the file.
extern SystemState currentState;
extern AnimationState animState;
extern unsigned long lastFrameTime;
extern int currentFrame;

// Number of frames per animation
const int IDLE_FRAMES = 2;
const int ALERT_FRAMES = 3;

// Constructor for SH1106 128x64 using Hardware I2C
U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// Unicode glyph codes for solid shapes
#define GLYPH_LEFT_SOLID     0x25c0   // ◀
#define GLYPH_RIGHT_SOLID    0x25b6   // ▶
#define GLYPH_DOWN_SOLID     0x25bc   // ▼
#define GLYPH_UP_SOLID       0x25b2   // ▲
#define GLYPH_CENTER_SOLID   0x25cf   // ●

// Unicode glyph codes for hollow shapes
#define GLYPH_LEFT_HOLLOW    0x25c1   // ◁
#define GLYPH_RIGHT_HOLLOW   0x25b7   // ▷
#define GLYPH_DOWN_HOLLOW    0x25bd   // ▽
#define GLYPH_UP_HOLLOW      0x25b3   // △
#define GLYPH_CENTER_HOLLOW  0x25cb   // ○

// Touch state variables
bool touchState_UP = false;
bool prevTouchState_UP = false;
bool touchState_DOWN = false;
bool prevTouchState_DOWN = false;
bool touchState_CENTER = false;
bool prevTouchState_CENTER = false;
bool touchState_LEFT = false;
bool prevTouchState_LEFT = false;
bool touchState_RIGHT = false;
bool prevTouchState_RIGHT = false;

// Flags for menu navigation
bool keysBlocked = false;
bool downPressedOnEnter = false;
bool centerPressedOnEnter = false;
bool blockTouchUntilRelease = false;
unsigned long menuInputUnlockAt = 0;



// Draw sprite centered on display
void drawSprite(const unsigned char* sprite) {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, SPRITE_WIDTH / 8, SPRITE_HEIGHT, sprite);
    u8g2.sendBuffer();
}

// Check for edge transitions - just pressed (rising edge)
bool wasPressed(bool current, bool previous) {
    return current && !previous;
}

// Check for just released (falling edge)
bool wasReleased(bool current, bool previous) {
    return !current && previous;
}

bool isAnyTouchPressed() {
    return touchState_UP || touchState_DOWN || touchState_CENTER || touchState_LEFT || touchState_RIGHT;
}



void handleMenuInput() {
    // Check for exit combo (Center + UP held)
    if (shouldExitMenu()) {
        saveAllSettings();  // Auto-save when exiting to idle
        beepC5(80);  // Sound feedback for exiting
        setState(STATE_IDLE);
        return;
    }
    
    // Check for back combo (Double-tap LEFT)
    if (shouldGoBack()) {
        menuGoBack();
        return;
    }
    
    // UP - select previous item (on release)
    if (wasReleased(touchState_UP, prevTouchState_UP)) {
        if (!keysBlocked) {
            menuSelectPrev();
        } else {
            keysBlocked = false;
        }
    }
    
    // DOWN - select next item (on release) - but ignore if it was part of the combo that entered menu
    if (wasReleased(touchState_DOWN, prevTouchState_DOWN)) {
        if (downPressedOnEnter) {
            downPressedOnEnter = false;
        } else if (!keysBlocked) {
            menuSelectNext();
        } else {
            keysBlocked = false;
        }
    }
    
    // CENTER - enter/select (on release) - but ignore if it was part of the combo that entered menu
    if (wasReleased(touchState_CENTER, prevTouchState_CENTER)) {
        // If Center was pressed as part of the Down+Center combo, ignore its first release
        if (centerPressedOnEnter) {
            // Center was part of the enter combo - clear the flag and ignore this release
            centerPressedOnEnter = false;
            Serial.println("CENTER released (was part of enter combo - ignored)");
        } else if (!keysBlocked) {
            menuEnter();
        } else {
            keysBlocked = false;
        }
    }
    
    // LEFT - decrease value (on release)
    if (wasReleased(touchState_LEFT, prevTouchState_LEFT)) {
        if (!keysBlocked) {
            menuAdjustValue(-1);
        } else {
            keysBlocked = false;
        }
    }
    
    // RIGHT - increase value (on release)
    if (wasReleased(touchState_RIGHT, prevTouchState_RIGHT)) {
        if (!keysBlocked) {
            menuAdjustValue(+1);
        } else {
            keysBlocked = false;
        }
    }
    
    // Render menu (always render even when QR code is displayed to keep display active)
    renderMenu(u8g2);
}

// Initialize touch pins
void initTouch() {
    Serial.println("Touch pins initialized: GPIO 3=Center, 4=Up, 5=Down, 6=Left, 7=Right");
}

void setup() {
    Serial.begin(115200);
    esp_task_wdt_init(10, false); // Ensure watchdog timer initialization works
    delay(1000);
    Serial.println("--- Booting ESP32-S3 N16R8 ---");
    Serial.println("Initializing system...");

    // Initialize touch pins
    initTouch();

    // Initialize I2C and display
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!Wire.begin()) {
        Serial.println("I2C Hardware Init Failed!");
        neopixelWrite(RGB_LED_PIN, 50, 0, 0); // Red for error
        while (1);
    }

    if (u8g2.begin()) {
        Serial.println("U8g2 initialized successfully on 8/9");
        neopixelWrite(RGB_LED_PIN, 0, 50, 0); // Green for success
    } else {
        Serial.println("SH1106 not found. Check address/wiring.");
        neopixelWrite(RGB_LED_PIN, 50, 25, 0); // Orange for "Display not found"
    }

    u8g2.setContrast(255); // Maximum brightness
    setState(STATE_IDLE);
    drawSpriteWithStats(u8g2, getCurrentSprite());

    Serial.println("Display initialized with sprite animation + stats");

    // Create WiFi command queue for cross-core communication
    wifiCommandQueue = xQueueCreate(10, sizeof(WifiCommand));
    
    // WiFi task runs on Core 0 (separate from display/UI)
    xTaskCreatePinnedToCore(
        wifiHandlerTask,
        "WiFiHandler",
        WIFI_TASK_STACK_SIZE,
        nullptr,
        WIFI_TASK_PRIORITY,
        nullptr,
        WIFI_TASK_CORE
    );
    Serial.println("[Setup] WiFiHandler task created on Core 0");

    xTaskCreatePinnedToCore(
        mainLoopTask,
        "MainLoop",
        8192,
        nullptr,
        1,
        nullptr,
        1  // Core 1
    );
    Serial.println("[Setup] Main loop task created on Core 1");
}

void loop() {
    delay(10);
}

// Main loop task pinned to Core 1
void mainLoopTask(void* param) {
    Serial.println("[MainLoop] Running on Core 1");
    SystemState previousState = currentState;

    while (true) {
        // Save previous touch states
        prevTouchState_UP = touchState_UP;
        prevTouchState_DOWN = touchState_DOWN;
        prevTouchState_CENTER = touchState_CENTER;
        prevTouchState_LEFT = touchState_LEFT;
        prevTouchState_RIGHT = touchState_RIGHT;

        // Read current touch states
        touchState_UP = touchRead(TOUCH_PIN_UP) > TOUCH_THRESHOLD;
        touchState_DOWN = touchRead(TOUCH_PIN_DOWN) > TOUCH_THRESHOLD;
        touchState_CENTER = touchRead(TOUCH_PIN_CENTER) > TOUCH_THRESHOLD;
        touchState_LEFT = touchRead(TOUCH_PIN_LEFT) > TOUCH_THRESHOLD;
        touchState_RIGHT = touchRead(TOUCH_PIN_RIGHT) > TOUCH_THRESHOLD;

        if (currentState != previousState) {
            blockTouchUntilRelease = true;
            menuInputUnlockAt = millis() + 150;
            previousState = currentState;
        }

        if (blockTouchUntilRelease) {
            if (!isAnyTouchPressed() && millis() >= menuInputUnlockAt) {
                blockTouchUntilRelease = false;
                keysBlocked = false;
                downPressedOnEnter = false;
                centerPressedOnEnter = false;
            } else {
                if (currentState == STATE_MENU) {
                    renderMenu(u8g2);
                } else {
                    updateDemi(u8g2);
                }
                delay(5);
                continue;
            }
        }

        // Check for menu enter combo (Down + Center)
        if (currentState == STATE_IDLE && touchState_CENTER && touchState_DOWN) {
            centerPressedOnEnter = true;
            downPressedOnEnter = true;
            keysBlocked = true;
            beepC5(80);
            setState(STATE_MENU);
        }

        if (currentState == STATE_MENU) {
            handleMenuInput();
        } else {
            updateDemi(u8g2);
        }
        
        delay(5);
        yield();
    }
}
