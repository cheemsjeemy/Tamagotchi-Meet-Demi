#include <Arduino.h>
#include <U8g2lib.h>
#include <Wire.h>
#include <WiFi.h>
#include <Preferences.h>
#include "DemiHandler.h"
#include "menu.h"
#include "Demi/Animations/sprite_idle.h"
#include "Demi/Animations/sprite_alert.h"
#include "Demi/Animations/sprite_sleep.h"
#include <esp_task_wdt.h>
#include "WiFiHandler.h"
#include "esp_partition.h"
#include <SPIFFS.h>
#include "miscellaneous/commands.h"
#include "RTCHandler.h"
#include "Payloads/Oscilloscope.h"
#include "Payloads/Timezones.h"
#include "captive_portal.h"


//DEMI IS A MALE CAT!!

bool _TestingController = false; 

// NTP constants
static const long gmtOffset_sec = 28800;
static const int daylightOffset_sec = 0;

// Forward declaration for tasks
void mainLoopTask(void* param);

// Forward declarations for I2C watchdog
void performHardReset();
bool attemptSoftRecovery();
void checkI2CHealth();

// Pins as requested
#define OLED_SDA_PIN 8
#define OLED_SCL_PIN 9
#define OLED_PWR_PIN 1  // NPN transistor to control OLED GND
#define RGB_LED_PIN 48 // Onboard LED for status

// Physical button pins
// Button definitions in menu.h (included via menu.h)

// Buzzer pin
#define BUZZER_PIN 40

// Note frequencies
#define NOTE_E5 659
#define NOTE_C5 523
#define NOTE_CS5 554  // C#5
#define NOTE_C2 65   // Low bass C
#define NOTE_B4 247  // B4 octave

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

void beepB4(uint16_t durationMs = 30) {
    beep(NOTE_B4, durationMs);
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

// Button state variables
bool btnState_UP = false;
bool prevBtnState_UP = false;
bool btnState_DOWN = false;
bool prevBtnState_DOWN = false;
bool btnState_CENTER = false;
bool prevBtnState_CENTER = false;
bool btnState_LEFT = false;
bool prevBtnState_LEFT = false;
bool btnState_RIGHT = false;
bool prevBtnState_RIGHT = false;
bool btnState_LB = false;
bool prevBtnState_LB = false;
bool btnState_RB = false;
bool prevBtnState_RB = false;

// Demi reset variables
bool demiResetWaiting = false;
bool demiResetReady = false;
bool showResetProgress = false;
unsigned long demiResetStartTime = 0;
bool aiDebugEnabled = false;  // Toggle for AI debug logging

// OLED hard reset variables
bool oledResetTimerStarted = false;
unsigned long oledResetStartTime = 0;

// I2C Watchdog variables
bool i2cWatchdogEnabled = true;
unsigned long lastI2CCheck = 0;
const unsigned long I2C_CHECK_INTERVAL = 2000;  // Check every 2 seconds
const int I2C_ERROR_THRESHOLD = 3;              // 3 consecutive failures before reset
int consecutiveI2CErrors = 0;
unsigned long lastSuccessfulI2C = 0;
bool softRecoveryAttempted = false;

// Button handler struct
struct Button {
    uint8_t pin;
    bool* state;
    bool* prevState;
    bool rawState;
    unsigned long lastChange;
    bool wasPressed;
    bool wasReleased;
};

Button buttons[] = {
    {BTN_UP, &btnState_UP, &prevBtnState_UP, false, 0, false, false},
    {BTN_DOWN, &btnState_DOWN, &prevBtnState_DOWN, false, 0, false, false},
    {BTN_CENTER, &btnState_CENTER, &prevBtnState_CENTER, false, 0, false, false},
    {BTN_LEFT, &btnState_LEFT, &prevBtnState_LEFT, false, 0, false, false},
    {BTN_RIGHT, &btnState_RIGHT, &prevBtnState_RIGHT, false, 0, false, false},
    {BTN_LB, &btnState_LB, &prevBtnState_LB, false, 0, false, false},
    {BTN_RB, &btnState_RB, &prevBtnState_RB, false, 0, false, false},
};

const unsigned long DEBOUNCE_MS = 30;

void updateButton(Button& btn) {
    bool reading = digitalRead(btn.pin) == LOW;
    
    if (reading != *btn.state) {
        bool prev = *btn.state;
        *btn.state = reading;
        
        btn.wasPressed = reading && !prev;
        btn.wasReleased = !reading && prev;
    }
}

void handleAllButtons() {
    for (auto& btn : buttons) {
        updateButton(btn);
    }
    
    // Crosstalk filter: if DOWN and CENTER pressed together, ignore both (crosstalk)
    bool downPressed = btnState_DOWN;
    bool centerPressed = btnState_CENTER;
    
    if (downPressed && centerPressed) {
        Serial.println("CROSSTALK DETECTED");
        for (auto& btn : buttons) {
            if (btn.pin == BTN_CENTER || btn.pin == BTN_DOWN) {
                btn.wasPressed = false;
                btn.wasReleased = false;
            }
        }
    }
}

// Flags for menu navigation
bool keysBlocked = false;
bool downPressedOnEnter = false;
bool centerPressedOnEnter = false;
bool blockTouchUntilRelease = false;
unsigned long menuInputUnlockAt = 0;
unsigned long systemBootTime = 0;
#define MENU_ENTER_DELAY_MS 2000  // Wait 2 seconds before menu combo works



// Fade-in animation for OLED display using hardware contrast control
void fadeInDisplay(uint16_t durationMs = 1500) { // Slowed down to see the start
    u8g2.setPowerSave(0); 
    u8g2.setContrast(5); // Force black start
    u8g2.sendBuffer(); 
    
    uint32_t startTime = millis();
    
    while (true) {
        uint32_t elapsed = millis() - startTime;
        if (elapsed >= durationMs) break;

        float progress = (float)elapsed / durationMs;

        // Sine Ease-In: This starts the slope at almost zero.
        // It stays in the "dim" values (1-20) for much longer.
        float ease = 1.0f - cos((progress * PI) / 2.0f); // Standard Ease-In
        
        // We square the ease to make it stay "near zero" even longer
        uint8_t contrast = (uint8_t)(ease * ease * 255.0f); 
        
        u8g2.setContrast(contrast);  
        
        delay(10); // Maintain 100fps
    }
    
    u8g2.setContrast(255); 
}
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

bool isAnyButtonPressed() {
    return btnState_UP || btnState_DOWN || btnState_CENTER || btnState_LEFT || btnState_RIGHT || btnState_LB || btnState_RB;
}

// --- I2C WATCHDOG FUNCTIONS ---
// Hard reset: power cycle the OLED via transistor
void performHardReset() {
    Serial.println("[OLED] Performing hard reset...");
    
    // 1. Stop I2C communication to prevent "ghost power" leaks
    pinMode(OLED_SDA_PIN, INPUT);
    pinMode(OLED_SCL_PIN, INPUT);
    
    // 2. Cut the Ground path via the NPN transistor
    digitalWrite(OLED_PWR_PIN, LOW);
    
    // 3. WAIT for large capacitor to drain completely
    delay(2000);
    
    // 4. Restore the Ground path
    digitalWrite(OLED_PWR_PIN, HIGH);
    delay(200); // Let voltage stabilize
    
    // 5. Re-initialize I2C and the display
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
    if (!Wire.begin()) {
        Serial.println("[OLED] Wire.begin() failed!");
        return;
    }
    
    if (u8g2.begin()) {
        Serial.println("[OLED] Recovered successfully!");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setDrawColor(1);
        u8g2.setCursor(0, 20);
        u8g2.print("OLED Recovered!");
        u8g2.sendBuffer();
        delay(1500);
    } else {
        Serial.println("[OLED] Recovery failed!");
    }
}

// Soft recovery: reinitialize I2C and display without power cycling
bool attemptSoftRecovery() {
    Serial.println("[I2C Watchdog] Attempting soft recovery...");
    
    Wire.end();
    delay(50);
    
    if (!Wire.begin()) {
        Serial.println("[I2C Watchdog] Wire.begin() failed in soft recovery!");
        return false;
    }
    Wire.setClock(100000);
    delay(100);
    
    if (u8g2.begin()) {
        Serial.println("[I2C Watchdog] Soft recovery successful!");
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x10_tf);
        u8g2.setDrawColor(1);
        u8g2.setCursor(0, 20);
        u8g2.print("I2C Recovered!");
        u8g2.sendBuffer();
        delay(1000);
        return true;
    } else {
        Serial.println("[I2C Watchdog] Soft recovery failed - display not responding");
        return false;
    }
}

// Check I2C bus health by probing the OLED
void checkI2CHealth() {
    if (!i2cWatchdogEnabled) return;
    
    Wire.beginTransmission(0x3C);
    uint8_t error = Wire.endTransmission();
    
    if (error == 0) {
        consecutiveI2CErrors = 0;
        lastSuccessfulI2C = millis();
        softRecoveryAttempted = false;
        return;
    }
    
    consecutiveI2CErrors++;
    
    static unsigned long lastErrorPrint = 0;
    if (millis() - lastErrorPrint > 1000) {
        Serial.printf("[I2C Watchdog] Error %d (count: %d/%d)\n", 
                     error, consecutiveI2CErrors, I2C_ERROR_THRESHOLD);
        lastErrorPrint = millis();
    }
    
    if (consecutiveI2CErrors >= I2C_ERROR_THRESHOLD) {
        Serial.printf("[I2C Watchdog] %d consecutive I2C errors - triggering recovery\n", 
                     consecutiveI2CErrors);
        
        if (!softRecoveryAttempted) {
            softRecoveryAttempted = true;
            if (attemptSoftRecovery()) {
                consecutiveI2CErrors = 0;
                return;
            }
        }
        
        Serial.println("[I2C Watchdog] Soft recovery failed, performing hard reset...");
        performHardReset();
        consecutiveI2CErrors = 0;
        softRecoveryAttempted = false;
    }
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
    
    // Check for RB = Back button
    if (wasReleased(btnState_RB, prevBtnState_RB)) {
        if (!keysBlocked) {
            menuGoBack();
        } else {
            keysBlocked = false;
        }
    }
    
    // UP - Page Up (fast scroll up) OR horizontal enter
    if (wasReleased(btnState_UP, prevBtnState_UP)) {
        if (menuState.isHorizontalMenu) {
            // Horizontal: enter selected submenu
            MenuItem* rootItems[] = { &menuDemi, &menuSettings, &menuMicrosoft, &menuPayloads };
            menuState.currentMenu = rootItems[menuState.horizontalIndex];
            menuState.selectedItem = menuState.currentMenu->firstChild;
            menuState.isHorizontalMenu = false;
            menuState.scrollOffset = 0;
            menuState.needsRedraw = true;
            beepC5(50);
        } else if (!keysBlocked) {
            menuSelectPrev();
        } else {
            keysBlocked = false;
        }
    }
    
    // DOWN - Page Down (fast scroll down) OR horizontal enter
    if (wasReleased(btnState_DOWN, prevBtnState_DOWN)) {
        if (menuState.isHorizontalMenu) {
            // Horizontal: enter selected submenu
            MenuItem* rootItems[] = { &menuDemi, &menuSettings, &menuMicrosoft, &menuPayloads };
            menuState.currentMenu = rootItems[menuState.horizontalIndex];
            menuState.selectedItem = menuState.currentMenu->firstChild;
            menuState.isHorizontalMenu = false;
            menuState.scrollOffset = 0;
            menuState.needsRedraw = true;
            beepC5(50);
        } else if (downPressedOnEnter) {
            downPressedOnEnter = false;
        } else if (!keysBlocked) {
            menuSelectNext();
        } else {
            keysBlocked = false;
        }
    }
    
    // CENTER - enter/select (on release) - but ignore if it was part of the combo that entered menu
    if (wasReleased(btnState_CENTER, prevBtnState_CENTER)) {
        // If Center was pressed as part of the enter combo, ignore its first release
        if (centerPressedOnEnter) {
            centerPressedOnEnter = false;
            Serial.println("CENTER released (was part of enter combo - ignored)");
        } else if (menuState.isHorizontalMenu) {
            // Horizontal: enter selected submenu
            MenuItem* rootItems[] = { &menuDemi, &menuSettings, &menuMicrosoft, &menuPayloads };
            menuState.currentMenu = rootItems[menuState.horizontalIndex];
            menuState.selectedItem = menuState.currentMenu->firstChild;
            menuState.isHorizontalMenu = false;
            menuState.scrollOffset = 0;
            menuState.needsRedraw = true;
            beepC5(50);  // Enter sound
        } else if (!keysBlocked) {
            menuEnter();
        } else {
            keysBlocked = false;
        }
    }
    
    // LEFT - slider decrease (on release) OR horizontal menu nav
    if (wasReleased(btnState_LEFT, prevBtnState_LEFT)) {
        if (menuState.isHorizontalMenu) {
            // Horizontal: move left (wrap around)
            menuState.horizontalIndex = (menuState.horizontalIndex + 3) % 4;  // Wrap to previous
            beepB4(30);  // B4 beep for navigation
            menuState.needsRedraw = true;
        } else if (!keysBlocked) {
            menuAdjustValue(-1);
        } else {
            keysBlocked = false;
        }
    }
    
    // RIGHT - slider increase (on release) OR horizontal menu nav
    if (wasReleased(btnState_RIGHT, prevBtnState_RIGHT)) {
        if (menuState.isHorizontalMenu) {
            // Horizontal: move right (wrap around)
            menuState.horizontalIndex = (menuState.horizontalIndex + 1) % 4;  // Wrap to next
            beepB4(30);  // B4 beep for navigation
            menuState.needsRedraw = true;
        } else if (!keysBlocked) {
            menuAdjustValue(+1);
        } else {
            keysBlocked = false;
        }
    }
    
    // Render menu (always render even when QR code is displayed to keep display active)
    renderMenu(u8g2);
}

// Initialize button pins
void initButtons() {
    pinMode(BTN_RB, INPUT_PULLUP);
    pinMode(BTN_RIGHT, INPUT_PULLUP);
    pinMode(BTN_DOWN, INPUT_PULLUP);
    pinMode(BTN_CENTER, INPUT_PULLUP);
    pinMode(BTN_UP, INPUT_PULLUP);
    pinMode(BTN_LEFT, INPUT_PULLUP);
    pinMode(BTN_LB, INPUT_PULLUP);
    Serial.println("Buttons initialized: GPIO 4=RB, 5=Right, 6=Down, 7=Center, 15=Up, 16=Left, 17=LB");
}



void printHealthStatus() {
    // 1. RAM Calculation
    uint32_t totalInt = ESP.getHeapSize();
    uint32_t freeInt = ESP.getFreeHeap();
    uint32_t usedInt = totalInt - freeInt;

    // 2. PSRAM Calculation
    uint32_t totalPsram = ESP.getPsramSize();
    uint32_t freePsram = ESP.getFreePsram();
    uint32_t usedPsram = totalPsram - freePsram;

    Serial.println("\n \n \n \n");

    Serial.println("\n--- LIVE MEMORY USAGE --- \n");

    Serial.println("[INTERNAL RAM]");
    Serial.printf("  USED: %6.2f KB | FREE: %6.2f KB | TOTAL: %.2f KB (%.1f%% Used)\n", 
        usedInt/1024.0, freeInt/1024.0, totalInt/1024.0, ((float)usedInt/totalInt)*100);

    Serial.println("\n \n[PSRAM (8MB R8)]");
    if (totalPsram > 0) {
        Serial.printf("  USED: %6.2f MB | FREE: %6.2f MB | TOTAL: %.2f MB (%.1f%% Used)\n", 
            usedPsram/(1024.0*1024.0), freePsram/(1024.0*1024.0), totalPsram/(1024.0*1024.0), ((float)usedPsram/totalPsram)*100);
    } else {
        Serial.println("  PSRAM not enabled!");
    }

    Serial.println("\n \n[FLASH PARTITION DETAILED USAGE (N16)]");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        float pSizeKB = p->size / 1024.0;
        float pSizeMB = p->size / (1024.0 * 1024.0);
        float pUsedKB = 0, pUsedMB = 0, pPerc = 0;

        // Logic to find "Used" based on room type
        if (String(p->label) == "app0") {
            pUsedKB = ESP.getSketchSize() / 1024.0;
            pUsedMB = pUsedKB / 1024.0;
            pPerc = (pUsedKB / pSizeKB) * 100;
        } 
        else if (String(p->label) == "spiffs") {
            if (SPIFFS.begin(true)) {
                pUsedKB = SPIFFS.usedBytes() / 1024.0;
                pUsedMB = pUsedKB / 1024.0;
                pPerc = (pUsedKB / pSizeKB) * 100;
            }
        }
        // app1, otadata, and nvs are system managed; showing as "Reserved/System"
        
        Serial.printf("  Room: %-10s | USED: %7.2f KB (%4.2f MB) | TOTAL: %7.2f KB (%4.2f MB) | %5.2f%%\n", 
            p->label, pUsedKB, pUsedMB, pSizeKB, pSizeMB, pPerc);
            
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);

    Serial.println("\n \n[SYSTEM SUMMARY]");
    Serial.printf("  Physical Chip Total: %.2f MB\n", ESP.getFlashChipSize()/(1024.0*1024.0));
    Serial.printf("  Internal Chip Temp:  %.2f °C\n", temperatureRead());
    Serial.printf("  CPU FREQUENCY: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("  SDK Version: %s\n", ESP.getSdkVersion());

    Serial.println("\n--- END OF HEALTH REPORT ---\n \n");

}




void setup() {
    Serial.begin(115200);
    delay(1000);
    
    // Check reset reason - tells us WHY the ESP32 reset
    esp_reset_reason_t resetReason = esp_reset_reason();
    
    // Use RGB LED to indicate reset type (visible even without Serial)
    // Red = Brownout, Blue = Panic/Crash, Green = Normal, Yellow = Watchdog
    switch (resetReason) {
        case ESP_RST_POWERON:
            Serial.println("\n=== [RESET REASON] POWERON - Normal startup ===\n");
            neopixelWrite(RGB_LED_PIN, 0, 50, 0); // Green
            delay(500);
            break;
        case ESP_RST_BROWNOUT:
            Serial.println("\n=== [RESET REASON] BROWNOUT - Voltage dropped! ===\n");
            neopixelWrite(RGB_LED_PIN, 50, 0, 0); // Red
            // Flash red 3 times to get attention
            for (int i = 0; i < 3; i++) {
                neopixelWrite(RGB_LED_PIN, 0, 0, 0);
                delay(200);
                neopixelWrite(RGB_LED_PIN, 50, 0, 0);
                delay(200);
            }
            break;
        case ESP_RST_PANIC:
            Serial.println("\n=== [RESET REASON] PANIC - Code crash! ===\n");
            neopixelWrite(RGB_LED_PIN, 0, 0, 50); // Blue
            delay(1000);
            break;
        case ESP_RST_TASK_WDT:
        case ESP_RST_INT_WDT:
            Serial.println("\n=== [RESET REASON] WATCHDOG TIMEOUT ===\n");
            neopixelWrite(RGB_LED_PIN, 50, 50, 0); // Yellow
            delay(1000);
            break;
        default:
            Serial.printf("\n=== [RESET REASON] UNKNOWN (%d) ===\n\n", resetReason);
            neopixelWrite(RGB_LED_PIN, 0, 50, 50); // Cyan
            delay(500);
            break;
    }
    
    esp_task_wdt_init(10, false); // Ensure watchdog timer initialization works
    Serial.println("--- Booting ESP32-S3 N16R8 ---");
    Serial.println("Initializing system...");

    // Initialize boot time for menu enter delay
    systemBootTime = millis();

    // Initialize button pins
    initButtons();

    // Turn on OLED power via NPN transistor
    pinMode(OLED_PWR_PIN, OUTPUT);
    digitalWrite(OLED_PWR_PIN, HIGH); // NPN: HIGH = ON
    delay(500); // Wait for capacitor to charge and OLED to stabilize

    // Initialize I2C and display
    Wire.setPins(OLED_SDA_PIN, OLED_SCL_PIN);
   
    
    if (!Wire.begin()) {
        Serial.println("I2C Hardware Init Failed!");
        neopixelWrite(RGB_LED_PIN, 50, 0, 0); // Red for error
        while (1);
    }
    
    // I2C Bus scan to detect connected devices
    Serial.println("[I2C] Scanning for devices on SDA=GPIO8, SCL=GPIO9...");
    byte i2cError = 0;
    byte nDevices = 0;
    for (byte address = 1; address < 127; address++) {
        Wire.beginTransmission(address);
        i2cError = Wire.endTransmission();
        if (i2cError == 0) {
            Serial.printf("  [I2C] Device found at address 0x%02X", address);
            if (address == 0x3C) {
                Serial.print(" (SH1106 OLED display)");
            }
            Serial.println();
            nDevices++;
        } else if (i2cError == 4) {
            Serial.printf("  [I2C] Unknown error at address 0x%02X\n", address);
        }
    }
    if (nDevices == 0) {
        Serial.println("  [I2C] No devices found - check wiring!");
    } else {
        Serial.printf("[I2C] Scan complete: %d device(s) found\n", nDevices);
    }
    
    Wire.setClock(100000); // Set I2C clock to 100kHz for stability
    if (u8g2.begin()) {
        delay(200);
        u8g2.setContrast(255); // Start at max contrast for fade-in
    
        Serial.println("U8g2 initialized successfully on 8/9");
        neopixelWrite(RGB_LED_PIN, 0, 50, 0); // Green for success
        
        // Smooth fade-in animation for display
        //fadeInDisplay(4000);  // 4000ms fade-in
    } else {
        Serial.println("SH1106 not found. Check address/wiring.");
        neopixelWrite(RGB_LED_PIN, 50, 25, 0); // Orange for "Display not found"
    }

    setState(STATE_IDLE);
    drawSpriteWithStats(u8g2, getCurrentSprite());

    // Load Demi's saved stats
    loadAll();
    Serial.println("Demi's stats loaded!");

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


    delay(5000);
     Serial.println("--- [ ESP32 HEALTH REPORT ] --- \n");
     printHealthStatus();   

    Serial.println("[Setup] Oscilloscope initialized");   

    initRTC();   

    Serial.println("\n \n");
    Serial.println("[MAIN] About to create DemiMoodModel...");
    Serial.flush();
    // Initialize Demi Neural Network - create local then assign
    DemiMoodModel tempModel;
    moodModel = tempModel;
    Serial.flush();
    Serial.println("[MAIN] DemiMoodModel created, checking ready...");
    Serial.flush();
    Serial.printf("✅ Demi AI Neural Network: %s\n", moodModel.isReady() ? "RUNNING" : "NOT READY");
    Serial.flush();
    
    Serial.println("\n-------- [END OF SETUP()] --------\n");

     Serial.println("----------------------------------------");

}



void loop() {
    // Check if you typed something in the Serial Monitor
   if (Serial.available() > 0) {
        String input = Serial.readStringUntil('\n');
        input.trim();

        if (input.length() > 0) {
            bool found = false;
            for (int i = 0; i < cmdCount; i++) {
                // Check primary name OR check alias
                if (input == commandTable[i].name || 
                   (strlen(commandTable[i].alias) > 0 && input == commandTable[i].alias)) {
                    
                    commandTable[i].action();
                    found = true;
                    break;
                }
            }
            if (!found) Serial.println("Invalid command. Type 'h' for help.");
    processCaptivePortalDNS();
}
}

}
// Main loop task pinned to Core 1
void mainLoopTask(void* param) {
    Serial.println("[MainLoop] Running on Core 1");
    SystemState previousState = currentState;

    while (true) {
        // Capture previous states before updating
        prevBtnState_UP = btnState_UP;
        prevBtnState_DOWN = btnState_DOWN;
        prevBtnState_CENTER = btnState_CENTER;
        prevBtnState_LEFT = btnState_LEFT;
        prevBtnState_RIGHT = btnState_RIGHT;
        prevBtnState_LB = btnState_LB;
        prevBtnState_RB = btnState_RB;

        // Update all buttons with debouncing
        handleAllButtons();

        // Controller testing - print button presses to serial
        if (_TestingController) {
            for (auto& btn : buttons) {
                if (btn.wasPressed) {
                    if (btn.pin == BTN_UP) Serial.println("BTN: UP");
                    else if (btn.pin == BTN_DOWN) Serial.println("BTN: DOWN");
                    else if (btn.pin == BTN_LEFT) Serial.println("BTN: LEFT");
                    else if (btn.pin == BTN_RIGHT) Serial.println("BTN: RIGHT");
                    else if (btn.pin == BTN_CENTER) Serial.println("BTN: CENTER");
                    else if (btn.pin == BTN_LB) Serial.println("BTN: LB");
                    else if (btn.pin == BTN_RB) Serial.println("BTN: RB");
                }
            }
        }

        // Check for OLED hard reset: UP+DOWN+LEFT+RIGHT held for 5 seconds
        if (btnState_UP && btnState_DOWN && btnState_LEFT && btnState_RIGHT) {
            if (!oledResetTimerStarted) {
                oledResetTimerStarted = true;
                oledResetStartTime = millis();
            } else if (millis() - oledResetStartTime >= 5000) {
                performHardReset();
                oledResetTimerStarted = false;
            }
        } else {
            oledResetTimerStarted = false;
        }

        // Periodic I2C health check (every 2 seconds)
        if (millis() - lastI2CCheck >= I2C_CHECK_INTERVAL) {
            checkI2CHealth();
            lastI2CCheck = millis();
        }

        if (currentState != previousState) {
            blockTouchUntilRelease = true;
            menuInputUnlockAt = millis() + 150;
            previousState = currentState;
        }

        if (blockTouchUntilRelease) {
            if (!isAnyButtonPressed() && millis() >= menuInputUnlockAt) {
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

        // Check for menu enter (RB button) - with delay after boot
        // DISABLED during reset waiting - RB is exclusively for reset confirmation
        if (currentState == STATE_IDLE && btnState_RB && !demiResetWaiting) {
            if (millis() - systemBootTime >= MENU_ENTER_DELAY_MS) {
                keysBlocked = true;
                beepC5(80);
                setState(STATE_MENU);
            }
        }

        // Handle reset waiting phase (waiting for RB to be pressed)
        if (demiResetWaiting && !demiResetReady) {
            // 60 second timeout to press RB
            if (millis() - demiResetStartTime > 60000) {
                demiResetWaiting = false;
                Serial.println("❌ Reset timed out - cancelled");
            } else if (btnState_RB) {
                // RB pressed - start the 5 second hold
                demiResetReady = true;
                showResetProgress = true;
                demiResetStartTime = millis();  // Reset timer for hold duration
            }
            // Block all other menu navigation during reset waiting
            keysBlocked = true;
        }

        // Reset progress screen - hold RB for 5 seconds
        if (showResetProgress && demiResetReady) {
            unsigned long elapsed = millis() - demiResetStartTime;
            unsigned long remaining = (elapsed >= 5000) ? 0 : (5000 - elapsed);
            int progress = (int)(elapsed * 100 / 5000);  // 0-100%
            
            // Clear and draw progress
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_ncenB14_tr);
            
            // Draw circular progress (arc)
            int cx = 64, cy = 28, r = 20;
            for (int i = 0; i < 360; i += 3) {
                if (i < progress * 3.6) {
                    float angle = i * PI / 180;
                    int x = cx + r * cos(angle);
                    int y = cy + r * sin(angle);
                    u8g2.drawPixel(x, y);
                }
            }
            
            // Countdown text
            char countStr[8];
            snprintf(countStr, sizeof(countStr), "%d.%d", remaining / 1000, (remaining % 1000) / 100);
            u8g2.setCursor(50, 58);
            u8g2.print(countStr);
            
            u8g2.sendBuffer();
            
            // Check if held long enough
            if (!btnState_RB) {
                // Released early - cancel
                showResetProgress = false;
                demiResetReady = false;
                Serial.println("Reset cancelled");
            } else if (elapsed >= 5000) {
                // Complete - reset stats
                hunger = 100; happiness = 100; energy = 100;
                health = 100; cleanliness = 100; isSleeping = false;
                ai = DemiAI();
                saveAll();
                
                // Show resetting message
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_ncenB14_tr);
                u8g2.setCursor(25, 35);
                u8g2.print("RESETTING");
                u8g2.sendBuffer();
                delay(1500);
                
                showResetProgress = false;
                demiResetReady = false;
                Serial.println("✅ Demi stats reset!");
            }
        }

        if (currentState == STATE_MENU) {
            if (oscilloscopeActive) {
                updateOscilloscope(u8g2, btnState_LEFT, btnState_RIGHT, 
                    wasPressed(btnState_LEFT, prevBtnState_LEFT), 
                    wasPressed(btnState_RIGHT, prevBtnState_RIGHT));
                if (wasReleased(btnState_RB, prevBtnState_RB)) {
                    stopOscilloscope();
                    menuState.needsRedraw = true;
                }
            } else {
                handleMenuInput();
            }
        } else {
            if (oscilloscopeActive) {
                updateOscilloscope(u8g2, btnState_LEFT, btnState_RIGHT,
                    wasPressed(btnState_LEFT, prevBtnState_LEFT),
                    wasPressed(btnState_RIGHT, prevBtnState_RIGHT));
                if (wasReleased(btnState_RB, prevBtnState_RB)) {
                    stopOscilloscope();
                }
            } else {
                // Wake up from sleep if any button pressed
                if (isSleeping && (btnState_UP || btnState_DOWN || btnState_CENTER || 
                                    btnState_LEFT || btnState_RIGHT || btnState_LB || btnState_RB)) {
                    isSleeping = false;
                    saveAll();
                    Serial.println("[Demi] Woke up!");
                }
                updateDemi(u8g2);
            }
        }
        delay(5);
        yield();
    }
}

