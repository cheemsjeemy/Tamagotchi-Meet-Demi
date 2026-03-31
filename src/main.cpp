#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>

#include "sprite_idle.h"
#include "sprite_alert.h"
#include "menu.h"

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
    // Higher frequencies need lower resolution to be achievable
    uint8_t dutyResolution = 8;
    if (frequency < 200) dutyResolution = 10;
    if (frequency < 100) dutyResolution = 12;
    
    // Setup LEDC channel 0 with calculated resolution
    ledcSetup(0, frequency, dutyResolution);
    ledcAttachPin(BUZZER_PIN, 0);
    ledcWrite(0, (1 << dutyResolution) / 2);  // 50% duty cycle
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

// Touch state tracking
bool touchState_UP = false;
bool touchState_DOWN = false;
bool touchState_LEFT = false;
bool touchState_RIGHT = false;
bool touchState_CENTER = false;

// Track if keys are blocked after entering menu (require press+release before working)
bool keysBlocked = false;

// Track which specific keys were pressed when entering menu
bool downPressedOnEnter = false;
bool centerPressedOnEnter = false;

// Track previous key states for edge detection
bool prevTouchState_UP = false;
bool prevTouchState_DOWN = false;
bool prevTouchState_LEFT = false;
bool prevTouchState_RIGHT = false;
bool prevTouchState_CENTER = false;

// Sprite dimensions (128x64 display)
#define SPRITE_WIDTH 128
#define SPRITE_HEIGHT 64

// Animation timing (ms per frame)
#define IDLE_FRAME_DELAY 300
#define ALERT_FRAME_DELAY 150

// System state (what mode the device is in)
enum SystemState {
    STATE_IDLE,     // Sprite animation running
    STATE_MENU,     // Menu system active
    STATE_ALERT     // Alert animation playing
};

SystemState currentState = STATE_IDLE;

// Animation state (which animation to play)
enum AnimationState {
    ANIM_IDLE,
    ANIM_ALERT
};

AnimationState animState = ANIM_IDLE;
unsigned long lastFrameTime = 0;
int currentFrame = 0;

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

// Function to get current sprite frame based on animation state
const unsigned char* getCurrentSprite() {
    if (animState == ANIM_IDLE) {
        if (currentFrame == 0) return IDLE_1;
        else return IDLE_2;
    } else {
        if (currentFrame == 0) return ALERT_1;
        else if (currentFrame == 1) return ALERT_2;
        else return ALERT_3;
    }
}

// Function to get frame count for current animation
int getFrameCount() {
    return (animState == ANIM_IDLE) ? IDLE_FRAMES : ALERT_FRAMES;
}

// Function to get frame delay for current animation
int getFrameDelay() {
    return (animState == ANIM_IDLE) ? IDLE_FRAME_DELAY : ALERT_FRAME_DELAY;
}

// Change system state
void setState(SystemState newState) {
    if (currentState != newState) {
        currentState = newState;
        
        // Update LED color based on system state
        if (newState == STATE_IDLE) {
            animState = ANIM_IDLE;
            neopixelWrite(RGB_LED_PIN, 0, 50, 0); // Green for idle
        } else if (newState == STATE_MENU) {
            neopixelWrite(RGB_LED_PIN, 0, 25, 50); // Cyan for menu
            initMenu(); // Initialize menu when entering
        } else if (newState == STATE_ALERT) {
            animState = ANIM_ALERT;
            neopixelWrite(RGB_LED_PIN, 50, 0, 0); // Red for alert
        }
        
        // Reset animation frame
        currentFrame = 0;
        lastFrameTime = millis();
    }
}

// Draw sprite centered on display
void drawSprite(const unsigned char* sprite) {
    u8g2.clearBuffer();
    u8g2.drawBitmap(0, 0, SPRITE_WIDTH / 8, SPRITE_HEIGHT, sprite);
    u8g2.sendBuffer();
}

// Draw sprite with touch indicators overlay
void drawSpriteWithIndicators(const unsigned char* sprite) {
    u8g2.clearBuffer();
    
    // Draw sprite first
    u8g2.drawBitmap(0, 0, SPRITE_WIDTH / 8, SPRITE_HEIGHT, sprite);
    
    // Draw touch indicator glyphs
    u8g2.setFont(u8g2_font_cu12_t_symbols);

    // Top Left: Left Triangle (GPIO 7 - LEFT)
    if (touchState_LEFT) {
        u8g2.drawGlyph(MARGIN, 17, GLYPH_LEFT_SOLID);
    } else {
        u8g2.drawGlyph(MARGIN, 17, GLYPH_LEFT_HOLLOW);
    }

    // Top Right: Right Triangle (GPIO 6 - RIGHT)
    if (touchState_RIGHT) {
        u8g2.drawGlyph(128 - MARGIN - 12, 17, GLYPH_RIGHT_SOLID);
    } else {
        u8g2.drawGlyph(128 - MARGIN - 12, 17, GLYPH_RIGHT_HOLLOW);
    }

    // Bottom Left: Down Triangle (GPIO 5 - DOWN)
    if (touchState_DOWN) {
        u8g2.drawGlyph(MARGIN, 59, GLYPH_DOWN_SOLID);
    } else {
        u8g2.drawGlyph(MARGIN, 59, GLYPH_DOWN_HOLLOW);
    }

    // Bottom Right: Up Triangle (GPIO 4 - UP)
    if (touchState_UP) {
        u8g2.drawGlyph(128 - MARGIN - 12, 59, GLYPH_UP_SOLID);
    } else {
        u8g2.drawGlyph(128 - MARGIN - 12, 59, GLYPH_UP_HOLLOW);
    }

    // Top Middle: Circle (GPIO 3 - CENTER)
    if (touchState_CENTER) {
        u8g2.drawGlyph(58, 17, GLYPH_CENTER_SOLID);
    } else {
        u8g2.drawGlyph(58, 17, GLYPH_CENTER_HOLLOW);
    }

    u8g2.sendBuffer();
}

// Check touch input and update state
void checkTouch() {
    // Read all touch pins
    uint32_t touchVal_LEFT = touchRead(TOUCH_PIN_LEFT);
    uint32_t touchVal_RIGHT = touchRead(TOUCH_PIN_RIGHT);
    uint32_t touchVal_UP = touchRead(TOUCH_PIN_UP);
    uint32_t touchVal_DOWN = touchRead(TOUCH_PIN_DOWN);
    uint32_t touchVal_CENTER = touchRead(TOUCH_PIN_CENTER);

    /*
    // DEBUG: Log touch states before update
    static int debugCounter = 0;
    if (debugCounter++ % 10 == 0) {
        Serial0.print("[checkTouch] prev: D="); Serial0.print(prevTouchState_DOWN);
        Serial0.print(" C="); Serial0.println(prevTouchState_CENTER);
    }
    */
   
    // Store previous state for edge detection
    prevTouchState_UP = touchState_UP;
    prevTouchState_DOWN = touchState_DOWN;
    prevTouchState_LEFT = touchState_LEFT;
    prevTouchState_RIGHT = touchState_RIGHT;
    prevTouchState_CENTER = touchState_CENTER;

    // Update touch states
    touchState_LEFT = (touchVal_LEFT > TOUCH_THRESHOLD);
    touchState_RIGHT = (touchVal_RIGHT > TOUCH_THRESHOLD);
    touchState_UP = (touchVal_UP > TOUCH_THRESHOLD);
    touchState_DOWN = (touchVal_DOWN > TOUCH_THRESHOLD);
    touchState_CENTER = (touchVal_CENTER > TOUCH_THRESHOLD);
}

// Check for edge transitions - just pressed (rising edge)
bool wasPressed(bool current, bool previous) {
    return current && !previous;
}

// Check for just released (falling edge)
bool wasReleased(bool current, bool previous) {
    return !current && previous;
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
            Serial0.println("CENTER released (was part of enter combo - ignored)");
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
    
    // Render menu
    renderMenu(u8g2);
}

// Initialize touch pins
void initTouch() {
    Serial0.println("Touch pins initialized: GPIO 3=Center, 4=Up, 5=Down, 6=Left, 7=Right");
}

void setup() {
    Serial0.begin(115200);
    delay(1000);
    Serial0.println("--- Booting ESP32-S3 N16R8 ---");
    Serial0.println("Initializing system...");

    // Initialize touch pins
    initTouch();

    // 1. Force the S3 to use GPIO 8 and 9 for I2C
    // WARNING: If the board hangs here, GPIO 8/9 are being used by Flash/PSRAM
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
    
    if (!Wire.begin()) {
        Serial0.println("I2C Hardware Init Failed!");
        neopixelWrite(RGB_LED_PIN, 50, 0, 0); // Red for error
        while(1);
    }

    // 2. Initialize U8g2
    if (u8g2.begin()) {
        Serial0.println("U8g2 initialized successfully on 8/9");
        neopixelWrite(RGB_LED_PIN, 0, 50, 0); // Green for success
    } else {
        Serial0.println("SH1106 not found. Check address/wiring.");
        neopixelWrite(RGB_LED_PIN, 50, 25, 0); // Orange for "Display not found"
    }

    // 3. Set initial display settings
    u8g2.setContrast(255); // Maximum brightness
    
    // Initialize animation
    lastFrameTime = millis();
    setState(STATE_IDLE);
    
    // Draw initial sprite
    drawSpriteWithIndicators(getCurrentSprite());
    
    Serial0.println("Display initialized with sprite animation + touch indicators");
    Serial0.println("Touch: Center=GPIO3, Up=GPIO4, Down=GPIO5, Left=GPIO6, Right=GPIO7");
    Serial0.println("Menu: Down + Center to enter, Center + Up (hold) to exit, Double-tap Left to go back");
}

void loop() {
    unsigned long currentTime = millis();
    // Check touch input
    checkTouch();
    
    if (currentState == STATE_IDLE || currentState == STATE_ALERT) {
        if (currentState == STATE_IDLE && shouldEnterMenu()) {
            Serial0.println("shouldEnterMenu() = TRUE - entering menu!");
            Serial0.print("  touchState_DOWN = "); Serial0.println(touchState_DOWN);
            Serial0.print("  touchState_CENTER = "); Serial0.println(touchState_CENTER);
            Serial0.print("  prevTouchState_DOWN = "); Serial0.println(prevTouchState_DOWN);
            Serial0.print("  prevTouchState_CENTER = "); Serial0.println(prevTouchState_CENTER);
            beepE5(80);  // Sound feedback
            setState(STATE_MENU);
            keysBlocked = true;
            
            // Record which keys were pressed when entering menu
            // We need to track this so we can ignore their release
            downPressedOnEnter = touchState_DOWN;
            centerPressedOnEnter = touchState_CENTER;
            
            Serial0.print("  recorded: downPressedOnEnter="); Serial0.println(downPressedOnEnter);
            Serial0.print("  recorded: centerPressedOnEnter="); Serial0.println(centerPressedOnEnter);
        } else {
            // Update animation frame
            if (currentTime - lastFrameTime >= (unsigned long)getFrameDelay()) {
                lastFrameTime = currentTime;
                currentFrame++;
                if (currentFrame >= getFrameCount()) {
                    currentFrame = 0;
                }
            }
            
            // Draw sprite with touch indicators
            drawSpriteWithIndicators(getCurrentSprite());
        }
    } else if (currentState == STATE_MENU) {
        // Handle menu input
        handleMenuInput();
    }
    
    // Small delay to prevent busy-waiting
    delay(5);
}