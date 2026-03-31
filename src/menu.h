#ifndef MENU_H
#define MENU_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <esp_task.h>

// External U8G2 display instance (defined in main.cpp)
extern U8G2_SH1106_128X64_NONAME_F_HW_I2C u8g2;

// Touch pin definitions (must match main.cpp)
#define MENU_TOUCH_PIN_UP 4
#define MENU_TOUCH_PIN_DOWN 5
#define MENU_TOUCH_PIN_LEFT 7
#define MENU_TOUCH_PIN_RIGHT 6
#define MENU_TOUCH_PIN_CENTER 3
#define MENU_TOUCH_THRESHOLD 50000

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
    MENU_PAIR_ESP32 // Special: Pair to ESP32 (AP/STA modes)
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
};

// Global menu state
extern MenuState menuState;

// Preferences instance for saving settings
extern Preferences preferences;

// WiFi scan result structure and arrays (defined in menu.cpp)
struct WifiScanResult {
    char ssid[32];
    int rssi;
    bool isConnected;
};

#define MAX_WIFI_SCAN_RESULTS 10
extern WifiScanResult scanResults[MAX_WIFI_SCAN_RESULTS];
extern int numScanResults;

// Initialize menu system
void initMenu();

// Buzzer functions (defined in main.cpp)
void beepE5(uint16_t durationMs);
void beepC5(uint16_t durationMs);
void beepCSharp(uint16_t durationMs);
void beepLowC(uint16_t durationMs);

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

// Select next item
void menuSelectNext();

// Select previous item
void menuSelectPrev();

// Toggle or adjust current item value
void menuAdjustValue(int direction); // -1 = left/decrease, +1 = right/increase

// Execute current item action
void menuExecute();

// Check if we should enter menu (combo: Down + Center pressed together)
bool shouldEnterMenu();

// Check if we should exit menu (combo: Center + Up held together)
bool shouldExitMenu();

// Check if we should go back (double-tap LEFT within 500ms)
bool shouldGoBack();

// Render menu to display
void renderMenu(U8G2& u8g2);

#endif // MENU_H