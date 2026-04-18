#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <esp_task.h>
#include "WiFiHandler.h"

// External U8G2 display instance (defined in main.cpp)
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// Button pin definitions (must match main.cpp)
#define BTN_UP 10
#define BTN_DOWN 6
#define BTN_LEFT 11
#define BTN_RIGHT 5
#define BTN_CENTER 7
#define BTN_LB 12
#define BTN_RB 4

// Menu item types
enum MenuItemType {
    MENU_FOLDER,    // Has sub-items - navigates to child menu
    MENU_ACTION,    // Executes callback function
    MENU_TOGGLE,    // On/Off boolean value
    MENU_SLIDER,    // Adjustable value (0-255)
    MENU_INPUT,     // Text input (for WiFi SSID/password, BT name)
    MENU_WIFI_TEST, // Special: WiFi connection test
    MENU_BT_TEST,   // Special: Bluetooth test
    MENU_WIFI_LIST, // Special: WiFi scan results list
    MENU_PAIR_ESP32, // Special: Pair to ESP32 (AP/STA modes)
    MENU_STATUS     // Status display (read-only)
};

// Forward declaration
struct MenuItem;

// Menu item structure
struct MenuItem {
    const char* name;           // Display name
    MenuItemType type;          // Item type
    MenuItem* parent;           // Parent menu (null = root)
    MenuItem* firstChild;       // First child (if folder)
    MenuItem* nextSibling;      // Next item at same level
    MenuItem* prevSibling;      // Previous item at same level
    
    // Value storage
    union {
        bool boolValue;         // For TOGGLE
        uint8_t sliderValue;    // For SLIDER (0-255)
        char* stringValue;      // For INPUT (allocated string)
    };
    
    // Callback for actions
    void (*callback)();         // Function to call when selected
    
    // Slider configuration
    uint8_t sliderMin;          // Minimum slider value
    uint8_t sliderMax;          // Maximum slider value
    
    // Input configuration (for INPUT type)
    uint8_t inputMaxLen;        // Maximum string length
    
    // Preferences key (for auto-save)
    const char* prefKey;        // Key for saving to Preferences
};

// =============================================================================
// Menu Builder Functions - Makes defining menu items easier
// =============================================================================
// Usage examples:
//   MenuItem item = menuFolder("Settings");
//   MenuItem item = menuAction("About", cbAbout);
//   MenuItem item = menuToggle("Enabled", cbToggle, "enabled_key");
//   MenuItem item = menuSlider("Brightness", 128, cbBrightness, "brightness_key");
// =============================================================================

inline MenuItem menuFolder(const char* name) {
    MenuItem item{};

    item.name = name;
    item.type = MENU_FOLDER;
    item.parent = nullptr;
    item.firstChild = nullptr;
    item.nextSibling = nullptr;
    item.prevSibling = nullptr;

    item.boolValue = false;
    item.callback = nullptr;
    item.sliderMin = 0;
    item.sliderMax = 255;
    item.sliderValue = 0;
    item.prefKey = nullptr;

    return item;
}

inline MenuItem menuAction(const char* name, void (*callback)()) {
    MenuItem item{};

    item.name = name;
    item.type = MENU_ACTION;
    item.parent = nullptr;
    item.firstChild = nullptr;
    item.nextSibling = nullptr;
    item.prevSibling = nullptr;

    item.boolValue = false;
    item.callback = callback;
    item.sliderMin = 0;
    item.sliderMax = 255;
    item.sliderValue = 0;
    item.prefKey = nullptr;

    return item;
}

inline MenuItem menuToggle(const char* name, void (*callback)(), const char* prefKey) {
    MenuItem item{};

    item.name = name;
    item.type = MENU_TOGGLE;
    item.parent = nullptr;
    item.firstChild = nullptr;
    item.nextSibling = nullptr;
    item.prevSibling = nullptr;

    item.boolValue = false;
    item.callback = callback;
    item.sliderMin = 0;
    item.sliderMax = 255;
    item.sliderValue = 0;
    item.prefKey = prefKey;

    return item;
}

inline MenuItem menuSlider(const char* name, uint8_t defaultVal, void (*callback)(), const char* prefKey) {
    MenuItem item{};

    item.name = name;
    item.type = MENU_SLIDER;
    item.parent = nullptr;
    item.firstChild = nullptr;
    item.nextSibling = nullptr;
    item.prevSibling = nullptr;

    item.sliderMin = 0;
    item.sliderMax = 255;
    item.sliderValue = defaultVal;

    item.callback = callback;
    item.prefKey = prefKey;

    return item;
}

inline MenuItem menuInput(const char* name, uint8_t maxLen, const char* prefKey) {
    MenuItem item{};

    item.name = name;
    item.type = MENU_INPUT;
    item.parent = nullptr;
    item.firstChild = nullptr;
    item.nextSibling = nullptr;
    item.prevSibling = nullptr;

    item.stringValue = nullptr;
    item.callback = nullptr;
    item.sliderMin = 0;
    item.sliderMax = 255;
    item.inputMaxLen = maxLen;
    item.prefKey = prefKey;

    return item;
}

inline MenuItem menuStatus(const char* name, void (*callback)()) {
    MenuItem item{};

    item.name = name;
    item.type = MENU_STATUS;
    item.parent = nullptr;
    item.firstChild = nullptr;
    item.nextSibling = nullptr;
    item.prevSibling = nullptr;

    item.boolValue = false;
    item.callback = callback;
    item.sliderMin = 0;
    item.sliderMax = 255;
    item.sliderValue = 0;
    item.prefKey = nullptr;

    return item;
}

// Menu system state
struct MenuState {
    MenuItem* currentMenu;       // Current menu being displayed
    MenuItem* selectedItem;      // Currently selected item
    MenuItem* lastSelectedItem;  // Last selected item (for Back navigation history)
    uint8_t scrollOffset;        // Scroll position for long menus
    bool needsRedraw;            // Flag to trigger redraw
    bool isEditing;             // True if editing a slider/input value
    uint8_t editCursorPos;      // Cursor position for input editing
    bool justEntered;           // True right after entering menu (blocks immediate Center)
    bool showQRCode;            // True if displaying QR code screen
    uint8_t horizontalIndex;   // Current icon index (0-3 for root items)
    bool isHorizontalMenu;     // true = root level (horizontal), false = submenu (vertical)
};

// Global menu state
extern MenuState menuState;

// Preferences instance for saving settings
extern Preferences preferences;

#define MAX_SAVED_NETWORKS 5

// Payload menu items
extern MenuItem menuOscilloscope;
extern WifiScanResult scanResults[MAX_WIFI_SCAN_RESULTS];
extern WifiScanResult savedNetworks[MAX_SAVED_NETWORKS];
extern int numScanResults;
extern int numSavedNetworks;
extern bool isWifiScanning;
extern bool isAutoConnectScan;
extern bool noSavedNetworksInRange;

// Root menu items for horizontal navigation
extern MenuItem menuSettings;
extern MenuItem menuDemi;
extern MenuItem menuMicrosoft;
extern MenuItem menuPayloads;
extern MenuState menuState;

// Initialize menu system
void initMenu();
void updateTotpInBackground();

// WiFi public API (defined in main.cpp)
void wifiConnect(const char* ssid, const char* password);
void wifiConnectAsync(const char* ssid, const char* password);
const char* getConnectionStatus();
void wifiScan();
void wifiSetMode(wifi_mode_t mode);
void wifiDisconnect();

// Buzzer functions (defined in main.cpp)
void beepE5(uint16_t durationMs);
void beepC5(uint16_t durationMs);
void beepCSharp(uint16_t durationMs);
void beepLowC(uint16_t durationMs);
void beepB4(uint16_t durationMs);

// Initialize preferences (load saved settings)
void initPreferences();

// Save all settings to preferences
void saveAllSettings();

// Load all settings from preferences
void loadAllSettings();

// Get first item in current menu
MenuItem* getFirstItem(MenuItem* menu);

// Get next visible item (considering scroll)
MenuItem* getNextVisibleItem(MenuItem* start);

// Get previous visible item
MenuItem* getPrevVisibleItem(MenuItem* start);

// Navigate to parent menu
void menuGoBack();

// Navigate to child menu (if folder)
void menuEnter();

// Render menu to display
void renderMenu(U8G2& u8g2);

// Display QR code on OLED
void displayQRCode(U8G2& u8g2);

// Reset QR code draw flag
void resetQRCodeState();

// Select next item
void menuSelectNext();

// Select previous item
void menuSelectPrev();

// Toggle or adjust current item value
void menuAdjustValue(int direction);

// Execute current item action
void menuExecute();

// Check if we should enter menu (combo: Down + Center pressed together)
bool shouldEnterMenu();

// Check if we should exit menu (combo: Center + Up held together)
bool shouldExitMenu();

// Check if we should go back (double-tap LEFT within 500ms)
bool shouldGoBack();

// Check if QR code is being displayed
inline bool isShowingQRCode() {
    return menuState.showQRCode;
}

inline bool isOscilloscopeActive() {
    extern volatile bool oscilloscopeActive;
    return oscilloscopeActive;
}

    // Check if WiFi scanning is in progress
    inline bool isScanningWifi() {
        return ::isWifiScanning;  // extern from menu.cpp
    }

// Forward declarations for TOTP functions
void initTotp();

#endif // MENU_H