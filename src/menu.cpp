#include "menu.h"
#include <string.h>
#include <WiFi.h>

// WiFi scan results array (max 10 networks)
// Struct WifiScanResult is defined in menu.h
WifiScanResult scanResults[MAX_WIFI_SCAN_RESULTS];
int numScanResults = 0;

// Scanning state flag - controls whether to show "SCANNING" instead of path
bool isWifiScanning = false;

// Preferences for saving settings
Preferences preferences;

// Global menu state
MenuState menuState = {};

// Forward declarations for callbacks
void cbBrightness();
void cbAbout();
void cbPetting();
void cbJumping();
void cbWash();
void cbSleep();
void cbWifiToggle();
void cbBluetoothToggle();
void cbMSAuth();
void cbFeed();
void cbWifiTest();
void cbBluetoothTest();
void cbSaveSettings();

// WiFi List callbacks
void cbWifiList();
void cbWifiScan();
void cbWifiForget();
void cbWifiConnect();

// Pair To ESP32 callbacks
void cbPairToEsp32();
void cbEsp32ToWifi();
void cbDeviceToEsp32();

// ============================================
// MENU TREE DEFINITION
// ============================================

// Root menu items
MenuItem menuSettings = {
    "Settings", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuDemi = {
    "Demi", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuWifi = {
    "Wifi", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

// WiFi Enabled toggle (inside Wifi folder)
MenuItem menuWifiEnabled = {
    "Enabled", MENU_TOGGLE, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbWifiToggle, 0, 255, 0, "wifi_enabled"
};

MenuItem menuBluetooth = {
    "Bluetooth", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

// Bluetooth Enabled toggle (inside Bluetooth folder)
MenuItem menuBtEnabled = {
    "Enabled", MENU_TOGGLE, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbBluetoothToggle, 0, 255, 0, "bt_enabled"
};

MenuItem menuMSAuth = {
    "MSAuth", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbMSAuth, 0, 255, 0, nullptr
};

// Settings submenu
MenuItem menuBrightness = {
    "Brightness", MENU_SLIDER, nullptr, nullptr, nullptr, nullptr, { .sliderValue = 128 }, cbBrightness, 0, 255, 0, "brightness"
};

MenuItem menuAbout = {
    "About", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbAbout, 0, 255, 0, nullptr
};

MenuItem menuSaveSettings = {
    "Save Settings", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbSaveSettings, 0, 255, 0, nullptr
};

// WiFi submenu items
MenuItem menuBtSettings = {
    "BT Settings", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuBtName = {
    "Device Name", MENU_INPUT, nullptr, nullptr, nullptr, nullptr, { .stringValue = nullptr }, nullptr, 0, 255, 32, "bt_name"
};

MenuItem menuBtTest = {
    "Test BT", MENU_BT_TEST, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbBluetoothTest, 0, 255, 0, nullptr
};

// Demi submenu - Play
MenuItem menuPlay = {
    "Play", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuPetting = {
    "Petting", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbPetting, 0, 255, 0, nullptr
};

MenuItem menuJumping = {
    "Jumping", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbJumping, 0, 255, 0, nullptr
};

// Demi submenu - Wash, Sleep, Feed
MenuItem menuWash = {
    "Wash", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbWash, 0, 255, 0, nullptr
};

MenuItem menuSleep = {
    "Sleep", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbSleep, 0, 255, 0, nullptr
};

MenuItem menuFeed = {
    "Feed", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuFridge = {
    "Fridge", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbFeed, 0, 255, 0, nullptr
};

// ============================================
// WIFI MENU ITEMS
// ============================================

// WiFi List - folder containing scanned networks
MenuItem menuWifiList = {
    "WiFi List", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

// Placeholder when no networks scanned yet
MenuItem menuWifiScanPlaceholder = {
    "[Tap to Scan]", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbWifiScan, 0, 255, 0, nullptr
};

// Placeholder during scanning - shows SCANNING in path
MenuItem menuWifiScanning = {
    "SCANNING...", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

// Dynamic network items (pre-allocated for all networks)
MenuItem menuWifiNetworks[MAX_WIFI_SCAN_RESULTS] = {};

// Network submenu items
MenuItem menuWifiForget = {
    "Forget", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbWifiForget, 0, 255, 0, nullptr
};

MenuItem menuWifiConnect = {
    "Connect", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbWifiConnect, 0, 255, 0, nullptr
};

MenuItem menuWifiIP = {
    "IP: --.-.-.-", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

// Pair To ESP32 - folder
MenuItem menuPairToEsp32 = {
    "Pair To ESP32", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

// ESP32 to WiFi source (shows portal link/QR)
MenuItem menuEsp32ToWifi = {
    "ESP32->WiFi", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuEsp32PortalLink = {
    "Link: 192.168.4.1", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuEsp32PortalQR = {
    "[Show QR Code]", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbEsp32ToWifi, 0, 255, 0, nullptr
};

// Device to ESP32 (shows QR code or SSID/password)
MenuItem menuDeviceToEsp32 = {
    "Device->ESP32", MENU_FOLDER, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuDeviceSSID = {
    "SSID: Demi-ESP32", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuDevicePassword = {
    "Pass: demiesp32", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, nullptr, 0, 255, 0, nullptr
};

MenuItem menuDeviceQRCode = {
    "[Show QR Code]", MENU_ACTION, nullptr, nullptr, nullptr, nullptr, { .boolValue = false }, cbDeviceToEsp32, 0, 255, 0, nullptr
};

// ============================================
// PREFERENCES FUNCTIONS
// ============================================

void initPreferences() {
    preferences.begin("demi settings", false);
    Serial0.println("Preferences initialized");
}

void saveAllSettings() {
    // Save WiFi toggle
    preferences.putBool("wifi_enabled", menuWifiEnabled.boolValue);

    // Save Bluetooth toggle
    preferences.putBool("bt_enabled", menuBtEnabled.boolValue);

    // Save Brightness
    preferences.putUChar("brightness", menuBrightness.sliderValue);

    // Save Bluetooth Name
    if (menuBtName.stringValue != nullptr) {
        preferences.putString("bt_name", menuBtName.stringValue);
    }

    Serial0.println("All settings saved to preferences");
}

void loadAllSettings() {
    // Load WiFi toggle
    if (preferences.isKey("wifi_enabled")) {
        menuWifiEnabled.boolValue = preferences.getBool("wifi_enabled", false);
    }

    // Load Bluetooth toggle
    if (preferences.isKey("bt_enabled")) {
        menuBtEnabled.boolValue = preferences.getBool("bt_enabled", false);
    }

    // Load Brightness
    if (preferences.isKey("brightness")) {
        menuBrightness.sliderValue = preferences.getUChar("brightness", 128);
    }

    Serial0.println("Settings loaded from preferences");
}

// ============================================
// LINK MENU ITEMS TOGETHER
// ============================================

void linkMenuItems() {
    // Link root items together as siblings
    menuSettings.nextSibling = &menuDemi;
    menuDemi.prevSibling = &menuSettings;
    menuDemi.nextSibling = &menuWifi;
    menuWifi.prevSibling = &menuDemi;
    menuWifi.nextSibling = &menuBluetooth;
    menuBluetooth.prevSibling = &menuWifi;
    menuBluetooth.nextSibling = &menuMSAuth;
    menuMSAuth.prevSibling = &menuBluetooth;

    // Settings children: Brightness -> About -> Save Settings
    menuSettings.firstChild = &menuBrightness;
    menuBrightness.parent = &menuSettings;
    menuBrightness.nextSibling = &menuAbout;
    menuAbout.prevSibling = &menuBrightness;
    menuAbout.nextSibling = &menuSaveSettings;
    menuSaveSettings.prevSibling = &menuAbout;
    menuSaveSettings.nextSibling = nullptr;

    // WiFi children: Enabled -> WiFi List -> Pair To ESP32
    menuWifi.firstChild = &menuWifiEnabled;
    menuWifiEnabled.parent = &menuWifi;
    menuWifiEnabled.nextSibling = &menuWifiList;
    menuWifiList.prevSibling = &menuWifiEnabled;
    menuWifiList.parent = &menuWifi;
    menuWifiList.nextSibling = &menuPairToEsp32;
    menuPairToEsp32.prevSibling = &menuWifiList;
    menuPairToEsp32.parent = &menuWifi;
    menuPairToEsp32.nextSibling = nullptr;

    // WiFi List children: Scan placeholder (networks added dynamically)
    menuWifiList.firstChild = &menuWifiScanPlaceholder;
    menuWifiScanPlaceholder.parent = &menuWifiList;
    menuWifiScanPlaceholder.nextSibling = nullptr;

    // Pair To ESP32 children: ESP32->WiFi -> Device->ESP32
    menuPairToEsp32.firstChild = &menuEsp32ToWifi;
    menuEsp32ToWifi.parent = &menuPairToEsp32;
    menuEsp32ToWifi.nextSibling = &menuDeviceToEsp32;
    menuDeviceToEsp32.prevSibling = &menuEsp32ToWifi;
    menuDeviceToEsp32.parent = &menuPairToEsp32;
    menuDeviceToEsp32.nextSibling = nullptr;

    // ESP32->WiFi children: Link -> QR Code
    menuEsp32ToWifi.firstChild = &menuEsp32PortalLink;
    menuEsp32PortalLink.parent = &menuEsp32ToWifi;
    menuEsp32PortalLink.nextSibling = &menuEsp32PortalQR;
    menuEsp32PortalQR.prevSibling = &menuEsp32PortalLink;
    menuEsp32PortalQR.parent = &menuEsp32ToWifi;
    menuEsp32PortalQR.nextSibling = nullptr;

    // Device->ESP32 children: SSID -> Password -> QR Code
    menuDeviceToEsp32.firstChild = &menuDeviceSSID;
    menuDeviceSSID.parent = &menuDeviceToEsp32;
    menuDeviceSSID.nextSibling = &menuDevicePassword;
    menuDevicePassword.prevSibling = &menuDeviceSSID;
    menuDevicePassword.parent = &menuDeviceToEsp32;
    menuDevicePassword.nextSibling = &menuDeviceQRCode;
    menuDeviceQRCode.prevSibling = &menuDevicePassword;
    menuDeviceQRCode.parent = &menuDeviceToEsp32;
    menuDeviceQRCode.nextSibling = nullptr;

    // Bluetooth children: Enabled toggle -> BT Settings folder
    menuBluetooth.firstChild = &menuBtEnabled;
    menuBtEnabled.parent = &menuBluetooth;
    menuBtEnabled.nextSibling = &menuBtSettings;
    menuBtSettings.prevSibling = &menuBtEnabled;
    menuBtSettings.parent = &menuBluetooth;
    menuBtSettings.nextSibling = nullptr;

    // BT Settings children: Device Name -> Test BT
    menuBtSettings.firstChild = &menuBtName;
    menuBtName.parent = &menuBtSettings;
    menuBtName.nextSibling = &menuBtTest;
    menuBtTest.prevSibling = &menuBtName;
    menuBtTest.nextSibling = nullptr;

    // Demi children: Play -> Wash -> Feed -> Sleep
    menuDemi.firstChild = &menuPlay;
    menuPlay.parent = &menuDemi;
    menuPlay.nextSibling = &menuWash;
    menuWash.prevSibling = &menuPlay;
    menuWash.parent = &menuDemi;
    menuWash.nextSibling = &menuFeed;
    menuFeed.prevSibling = &menuWash;
    menuFeed.parent = &menuDemi;
    menuFeed.nextSibling = &menuSleep;
    menuSleep.prevSibling = &menuFeed;
    menuSleep.parent = &menuDemi;
    menuSleep.nextSibling = nullptr;

    // Play children: Petting -> Jumping
    menuPlay.firstChild = &menuPetting;
    menuPetting.parent = &menuPlay;
    menuPetting.nextSibling = &menuJumping;
    menuJumping.prevSibling = &menuPetting;
    menuJumping.parent = &menuPlay;
    menuJumping.nextSibling = nullptr;

    // Feed children: Fridge
    menuFeed.firstChild = &menuFridge;
    menuFridge.parent = &menuFeed;
    menuFridge.nextSibling = nullptr;

    // Network items: Initialize each with Forget -> Connect -> IP structure
    for (int i = 0; i < MAX_WIFI_SCAN_RESULTS; i++) {
        menuWifiNetworks[i].type = MENU_FOLDER;
        menuWifiNetworks[i].name = "";  // Will be set dynamically from scan results
        menuWifiNetworks[i].firstChild = &menuWifiForget;
        menuWifiNetworks[i].parent = &menuWifiList;
        menuWifiNetworks[i].nextSibling = nullptr;  // Will be linked dynamically
        menuWifiNetworks[i].prevSibling = nullptr;  // Will be linked dynamically
    }

    // Network children: Forget -> Connect -> IP (shared by all network items)
    menuWifiForget.parent = nullptr;  // Will be set when network is selected
    menuWifiForget.nextSibling = &menuWifiConnect;
    menuWifiConnect.prevSibling = &menuWifiForget;
    menuWifiConnect.parent = nullptr;  // Will be set when network is selected
    menuWifiConnect.nextSibling = &menuWifiIP;
    menuWifiIP.prevSibling = &menuWifiConnect;
    menuWifiIP.parent = nullptr;  // Will be set when network is selected
    menuWifiIP.nextSibling = nullptr;
}

// ============================================
// UPDATE WIFI LIST BASED ON SCAN STATE
// ============================================

// Static storage for network names (to avoid modifying const char*)
static char networkNames[MAX_WIFI_SCAN_RESULTS][33];
// Static storage for IP address display
static char ipAddress[16];

void updateWifiListForScanState() {
    if (isWifiScanning) {
        // During scan - show scanning indicator
        menuWifiList.firstChild = &menuWifiScanning;
        menuWifiScanning.parent = &menuWifiList;
        menuWifiScanning.nextSibling = nullptr;
    } else if (numScanResults > 0) {
        // Networks found - link ALL networks as siblings
        // Update each network item with its SSID
        for (int i = 0; i < numScanResults && i < MAX_WIFI_SCAN_RESULTS; i++) {
            // Copy SSID to our buffer
            snprintf(networkNames[i], sizeof(networkNames[i]), "%s", scanResults[i].ssid);
            menuWifiNetworks[i].name = networkNames[i];

            // Set parent for network children
            menuWifiForget.parent = &menuWifiNetworks[i];
            menuWifiConnect.parent = &menuWifiNetworks[i];
            menuWifiIP.parent = &menuWifiNetworks[i];

            // Link siblings properly
            if (i == 0) {
                // First network is the first child of WiFi List
                menuWifiList.firstChild = &menuWifiNetworks[i];
                menuWifiNetworks[i].prevSibling = nullptr;
            } else {
                menuWifiNetworks[i].prevSibling = &menuWifiNetworks[i - 1];
                menuWifiNetworks[i - 1].nextSibling = &menuWifiNetworks[i];
            }

            // Last network links to rescan option
            if (i == numScanResults - 1) {
                menuWifiNetworks[i].nextSibling = &menuWifiScanPlaceholder;
                menuWifiScanPlaceholder.prevSibling = &menuWifiNetworks[i];
            }
        }

        // Update placeholder to show rescan option
        menuWifiScanPlaceholder.name = "[Rescan]";
        menuWifiScanPlaceholder.parent = &menuWifiList;
        menuWifiScanPlaceholder.nextSibling = nullptr;

        // Make sure all networks before numScanResults have proper nextSibling
        // (networks beyond numScanResults should be hidden)
        for (int i = 0; i < MAX_WIFI_SCAN_RESULTS; i++) {
            if (i >= numScanResults) {
                // Hide unused network items by unlinking them
                menuWifiNetworks[i].nextSibling = nullptr;
                menuWifiNetworks[i].prevSibling = nullptr;
            }
        }
    } else {
        // No results - show tap to scan
        menuWifiScanPlaceholder.name = "[Tap to Scan]";
        menuWifiList.firstChild = &menuWifiScanPlaceholder;
        menuWifiScanPlaceholder.parent = &menuWifiList;
        menuWifiScanPlaceholder.nextSibling = nullptr;
    }
}

// ============================================
// MENU STATE FUNCTIONS
// ============================================

void initMenu() {
    // Link all menu items
    linkMenuItems();

    // Start at root
    menuState.currentMenu = nullptr;
    menuState.selectedItem = &menuSettings;
    menuState.scrollOffset = 0;
    menuState.needsRedraw = true;
    menuState.isEditing = false;
    menuState.justEntered = false;

    // Initialize preferences and load saved settings
    initPreferences();
    loadAllSettings();
}

MenuItem* getFirstItem(MenuItem* menu) {
    if (menu == nullptr) {
        return &menuSettings;
    }

    // Update WiFi list based on scan state
    if (menu == &menuWifiList) {
        updateWifiListForScanState();
    }

    return menu->firstChild;
}

MenuItem* getNextVisibleItem(MenuItem* item) {
    if (!item) return nullptr;
    return item->nextSibling;
}

MenuItem* getPrevVisibleItem(MenuItem* item) {
    if (!item) return nullptr;
    return item->prevSibling;
}

void menuGoBack() {
    // Don't go back when editing - must press Center first
    if (menuState.isEditing) {
        return;
    }
    if (menuState.currentMenu == nullptr) {
        // Already at root
        return;
    }

    // Get the parent of current menu
    MenuItem* parent = menuState.currentMenu->parent;

    // Save current folder before navigating away
    MenuItem* previousFolder = menuState.currentMenu;

    if (parent) {
        // Going back to a parent folder
        // Store the folder we came from
        menuState.lastSelectedItem = previousFolder;

        // Go to parent folder
        menuState.currentMenu = parent;

        // Restore the item we had selected in this parent (the folder we came from)
        menuState.selectedItem = previousFolder;
    } else {
        // Going back to root
        // Store the folder we came from so we can return to it later
        menuState.lastSelectedItem = previousFolder;

        menuState.currentMenu = nullptr;

        // Restore the item we had selected at root (the folder we came from)
        menuState.selectedItem = previousFolder;
    }

    menuState.scrollOffset = 0;
    menuState.isEditing = false;
    menuState.needsRedraw = true;
    beepC5(80);
}

void menuEnter() {
    // Can't enter/execute when editing
    if (menuState.isEditing) {
        menuState.isEditing = false;
        menuState.needsRedraw = true;
        saveAllSettings();
        return;
    }
    if (!menuState.selectedItem) return;

    if (menuState.selectedItem->type == MENU_FOLDER && menuState.selectedItem->firstChild) {
        // Save current selection as history
        menuState.lastSelectedItem = menuState.selectedItem;
        // Enter the folder
        menuState.currentMenu = menuState.selectedItem;
        menuState.selectedItem = menuState.selectedItem->firstChild;
        menuState.scrollOffset = 0;
        menuState.isEditing = false;
        menuState.needsRedraw = true;
        beepE5(80);
    } else if (menuState.selectedItem->type == MENU_ACTION) {
        if (menuState.selectedItem->callback) {
            beepE5(50);
            menuState.selectedItem->callback();
        }
    } else if (menuState.selectedItem->type == MENU_TOGGLE) {
        menuState.selectedItem->boolValue = !menuState.selectedItem->boolValue;
        menuState.needsRedraw = true;
        beepCSharp(50);
        saveAllSettings();
    } else if (menuState.selectedItem->type == MENU_SLIDER) {
        menuState.isEditing = !menuState.isEditing;
        menuState.needsRedraw = true;
    }
}

void menuSelectNext() {
    if (menuState.isEditing) return;
    if (!menuState.selectedItem) return;

    MenuItem* next = getNextVisibleItem(menuState.selectedItem);
    if (next) {
        menuState.selectedItem = next;

        uint8_t maxVisible = 4;
        uint8_t pos = 0;
        MenuItem* checkItem = getFirstItem(menuState.currentMenu);
        while (checkItem && checkItem != menuState.selectedItem) {
            pos++;
            checkItem = getNextVisibleItem(checkItem);
        }

        if (pos >= maxVisible && menuState.scrollOffset < pos) {
            menuState.scrollOffset = pos - (maxVisible - 1);
        }

        menuState.needsRedraw = true;
    }
}

void menuSelectPrev() {
    if (menuState.isEditing) return;
    if (!menuState.selectedItem) return;

    uint8_t pos = 0;
    MenuItem* checkItem = getFirstItem(menuState.currentMenu);
    while (checkItem && checkItem != menuState.selectedItem) {
        pos++;
        checkItem = getNextVisibleItem(checkItem);
    }

    uint8_t maxVisible = 4;
    if (menuState.scrollOffset > 0 && pos < menuState.scrollOffset + 2) {
        menuState.scrollOffset--;
        menuState.needsRedraw = true;
        return;
    }

    MenuItem* prev = getPrevVisibleItem(menuState.selectedItem);
    if (prev) {
        menuState.selectedItem = prev;
        menuState.needsRedraw = true;
    }
}

void menuAdjustValue(int direction) {
    if (!menuState.selectedItem) return;

    if (menuState.selectedItem->type == MENU_SLIDER && menuState.isEditing) {
        int newValue = (int)menuState.selectedItem->sliderValue + (direction * 5);
        if (newValue < (int)menuState.selectedItem->sliderMin) {
            newValue = menuState.selectedItem->sliderMin;
        }
        if (newValue > (int)menuState.selectedItem->sliderMax) {
            newValue = menuState.selectedItem->sliderMax;
        }
        menuState.selectedItem->sliderValue = (uint8_t)newValue;
        menuState.needsRedraw = true;
    }
}

void menuExecute() {
    if (!menuState.selectedItem) return;

    if (menuState.selectedItem->type == MENU_FOLDER) {
        menuEnter();
    } else if (menuState.selectedItem->type == MENU_ACTION) {
        if (menuState.selectedItem->callback) {
            menuState.selectedItem->callback();
        }
    }
}

// ============================================
// COMBO KEY DETECTION
// ============================================

bool isKeyPressed(uint8_t pin) {
    uint32_t val = touchRead(pin);
    return (val > MENU_TOUCH_THRESHOLD);
}

bool shouldEnterMenu() {
    return isKeyPressed(MENU_TOUCH_PIN_DOWN) && isKeyPressed(MENU_TOUCH_PIN_CENTER);
}

bool shouldExitMenu() {
    static unsigned long holdStart = 0;

    if (isKeyPressed(MENU_TOUCH_PIN_CENTER) && isKeyPressed(MENU_TOUCH_PIN_UP)) {
        if (holdStart == 0) {
            holdStart = millis();
        } else if (millis() - holdStart >= 500) {
            holdStart = 0;
            return true;
        }
    } else {
        holdStart = 0;
    }
    return false;
}

bool shouldGoBack() {
    static unsigned long lastLeftPress = 0;
    static bool leftWasPressed = false;

    bool leftPressed = isKeyPressed(MENU_TOUCH_PIN_LEFT);
    unsigned long now = millis();

    if (leftWasPressed && !leftPressed) {
        if (now - lastLeftPress < 500) {
            lastLeftPress = 0;
            leftWasPressed = false;
            return true;
        }
        lastLeftPress = now;
    }

    leftWasPressed = leftPressed;
    return false;
}

// ============================================
// MENU RENDERING
// ============================================

void renderMenu(U8G2& u8g2) {
    bool showCursor = (millis() / 400) % 2;
    static bool lastBlinkState = false;

    if (!menuState.needsRedraw && (showCursor == lastBlinkState)) return;
    lastBlinkState = showCursor;

    u8g2.clearBuffer();

    // === 1. Draw Directory Path (Top) ===
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.setDrawColor(1);

    // Check if WiFi is currently scanning - show animated "Scanning..." instead of path
    if (isWifiScanning) {
        // Animated scanning text that cycles: "Scanning." -> "Scanning.." -> "Scanning..."
        static const char* scanDots[] = {".", "..", "..."};
        uint8_t dotIndex = (millis() / 500) % 3;  // Change every 500ms
        char scanText[16];
        snprintf(scanText, sizeof(scanText), "Scanning%s", scanDots[dotIndex]);
        u8g2.drawStr(0, 8, scanText);
        u8g2.drawLine(0, 12, 127, 12);
    } else {
        // Build full path
        char pathBuf[48] = "C:/";
        if (menuState.currentMenu) {
            const char* pathParts[10];
            MenuItem* current = menuState.currentMenu;
            uint8_t count = 0;
            while (current && count < 10) {
                pathParts[count++] = current->name;
                current = current->parent;
            }

            for (int i = count - 1; i >= 0; i--) {
                strncat(pathBuf, pathParts[i], sizeof(pathBuf) - strlen(pathBuf) - 1);
                if (i > 0) {
                    strncat(pathBuf, "/", sizeof(pathBuf) - strlen(pathBuf) - 1);
                }
            }
        }

        // Truncate if too long
        if (strlen(pathBuf) > 16) {
            char shortPath[20] = "../";
            uint16_t len = strlen(pathBuf);
            uint16_t startPos = (len > 14) ? (len - 14) : 0;
            strncat(shortPath, &pathBuf[startPos], 13);
            strncpy(pathBuf, shortPath, sizeof(pathBuf));
        }

        u8g2.drawStr(0, 8, pathBuf);
        u8g2.drawLine(0, 12, 127, 12);
    }

    // === 2. Setup Menu Font ===
    u8g2.setFont(u8g2_font_6x10_tf);
    uint8_t startY = 14;
    uint8_t lineHeight = 10;
    uint8_t maxVisible = 5;
    uint8_t visibleCount = 0;

    // === 3. Scroll Logic ===
    MenuItem* item = getFirstItem(menuState.currentMenu);
    uint8_t skipCount = menuState.scrollOffset;
    while (item && skipCount > 0) {
        item = getNextVisibleItem(item);
        skipCount--;
    }

    // === 4. Draw Items ===
    while (item && visibleCount < maxVisible) {
        bool isSelected = (item == menuState.selectedItem);
        uint8_t rowY = startY + (visibleCount * lineHeight);
        uint8_t textY = rowY + 8;
        uint8_t textX = 12;

        if (isSelected) {
            u8g2.setDrawColor(1);
            u8g2.drawBox(0, rowY, 128, lineHeight);
            u8g2.setDrawColor(0);

            if (showCursor) {
                u8g2.drawStr(2, textY, ">");
            }
        } else {
            u8g2.setDrawColor(1);
        }

        if (item->type == MENU_TOGGLE) {
            u8g2.drawStr(textX + 2, textY, item->name);
            u8g2.drawStr(100, textY, item->boolValue ? "[X]" : "[ ]");
        }
        else if (item->type == MENU_SLIDER) {
            if (menuState.isEditing && isSelected) {
                uint8_t barWidth = 60;
                uint8_t progress = (uint16_t)(item->sliderValue * barWidth) / 255;
                u8g2.drawFrame(textX + 2, rowY + 2, barWidth, 6);
                u8g2.drawBox(textX + 2, rowY + 2, progress, 6);

                char percent[8];
                itoa((item->sliderValue * 100) / 255, percent, 10);
                strcat(percent, "%");
                u8g2.drawStr(textX + barWidth + 10, textY, percent);
            } else {
                u8g2.drawStr(textX + 2, textY, item->name);
                char val[8];
                itoa(item->sliderValue, val, 10);
                u8g2.drawStr(100, textY, val);
            }
        }
        else if (item->type == MENU_FOLDER) {
            u8g2.drawStr(textX, textY, ">");
            u8g2.drawStr(textX + 8, textY, item->name);
        }
        else if (item->type == MENU_ACTION) {
            u8g2.drawStr(textX, textY, "*");
            u8g2.drawStr(textX + 8, textY, item->name);
        }
        else {
            u8g2.drawStr(textX + 2, textY, item->name);
        }

        u8g2.setDrawColor(1);
        item = getNextVisibleItem(item);
        visibleCount++;
    }

    u8g2.sendBuffer();
    menuState.needsRedraw = false;
}

// ============================================
// CALLBACK FUNCTIONS
// ============================================

void cbBrightness() {
    Serial0.println("Menu: Brightness adjusted");
}

void cbAbout() {
    Serial0.println("Menu: About selected");
}

void cbPetting() {
    Serial0.println("Menu: Petting action!");
}

void cbJumping() {
    Serial0.println("Menu: Jumping action!");
}

void cbWash() {
    Serial0.println("Menu: Wash action!");
}

void cbSleep() {
    Serial0.println("Menu: Sleep action!");
}

void cbFeed() {
    Serial0.println("Menu: Feed action!");
}

void cbWifiToggle() {
    menuWifiEnabled.boolValue = !menuWifiEnabled.boolValue;
    Serial0.print("Menu: WiFi toggled ");
    Serial0.println(menuWifiEnabled.boolValue ? "ON" : "OFF");

    if (menuWifiEnabled.boolValue) {
        WiFi.mode(WIFI_STA);
        Serial0.println("  WiFi mode enabled (STA)");
    } else {
        WiFi.disconnect();
        WiFi.mode(WIFI_OFF);
        Serial0.println("  WiFi disabled");
    }

    saveAllSettings();
    menuState.needsRedraw = true;
}

void cbBluetoothToggle() {
    menuBtEnabled.boolValue = !menuBtEnabled.boolValue;
    Serial0.print("Menu: Bluetooth toggled ");
    Serial0.println(menuBtEnabled.boolValue ? "ON" : "OFF");
    saveAllSettings();
    menuState.needsRedraw = true;
}

void cbMSAuth() {
    Serial0.println("Menu: MSAuth selected");
}

void cbSaveSettings() {
    saveAllSettings();
    Serial0.println("Menu: Settings saved!");
    menuState.needsRedraw = true;
}

void cbWifiTest() {
    Serial0.println("Menu: WiFi Test action!");
    if (!menuWifiEnabled.boolValue) {
        Serial0.println("  WiFi is OFF - enable first");
    }
}

void cbBluetoothTest() {
    Serial0.println("Menu: Bluetooth Test action!");
    Serial0.print("  Device Name: ");
    if (menuBtName.stringValue != nullptr) {
        Serial0.println(menuBtName.stringValue);
    } else {
        Serial0.println("(not set)");
    }

    if (!menuBtEnabled.boolValue) {
        Serial0.println("  Bluetooth is OFF - enable first");
    }
}

// ============================================
// WIFI LIST CALLBACKS
// ============================================

void cbWifiList() {
    // This is called when entering WiFi List folder
    Serial0.println("Menu: WiFi List entered");
    menuState.needsRedraw = true;
}

void cbWifiScan() {
    Serial0.println("Menu: WiFi scan triggered");

    if (!menuWifiEnabled.boolValue) {
        Serial0.println("  WiFi is OFF - enable first");
        beepLowC(100);
        return;
    }

    // Set scanning flag - this will show "SCANNING..." in the path
    isWifiScanning = true;
    menuState.needsRedraw = true;

    // Enable WiFi in STA mode if not already on
    if (WiFi.getMode() == WIFI_OFF) {
        WiFi.mode(WIFI_STA);
        delay(100);
    }

    Serial0.println("  Starting WiFi scan...");

    // Run WiFi scan on Core 0
    xTaskCreatePinnedToCore(
        [](void* param) {
            Serial0.println("    [WiFi Task] Scanning...");
            int n = WiFi.scanNetworks();
            Serial0.print("    [WiFi Task] Found ");
            Serial0.print(n);
            Serial0.println(" networks");

            // Store results
            numScanResults = min(n, MAX_WIFI_SCAN_RESULTS);
            for (int i = 0; i < numScanResults; i++) {
                strncpy(scanResults[i].ssid, WiFi.SSID(i).c_str(), 32);
                scanResults[i].ssid[32] = '\0';
                scanResults[i].rssi = WiFi.RSSI(i);
                scanResults[i].isConnected = (WiFi.status() == WL_CONNECTED &&
                    WiFi.SSID(i) == WiFi.SSID());
                Serial0.print("    [WiFi] ");
                Serial0.println(scanResults[i].ssid);
            }

            // Clear scanning flag
            isWifiScanning = false;

            // Update menu display
            menuState.needsRedraw = true;
            Serial0.println("    [WiFi Task] Done!");
            vTaskDelete(nullptr);
        },
        "wifi_scan",
        4096,
        nullptr,
        1,
        nullptr,
        0
    );
}

void cbWifiConnect() {
    Serial0.println("Menu: WiFi Connect action!");

    if (!menuWifiEnabled.boolValue) {
        Serial0.println("  WiFi is OFF - enable first");
        beepLowC(100);
        return;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial0.println("  Already connected to: ");
        Serial0.print("    SSID: ");
        Serial0.println(WiFi.SSID());
        Serial0.print("    IP: ");
        Serial0.println(WiFi.localIP());

        // Update IP display
        snprintf(ipAddress, sizeof(ipAddress), "IP: %s", WiFi.localIP().toString().c_str());
        menuWifiIP.name = ipAddress;
    } else {
        Serial0.println("  Not connected");
        Serial0.println("  Use Pair To ESP32 -> Device->ESP32 for setup");
    }

    menuState.needsRedraw = true;
}

void cbWifiForget() {
    Serial0.println("Menu: WiFi Forget action!");

    WiFi.disconnect(true);
    preferences.remove("wifi_ssid");
    preferences.remove("wifi_pass");

    // Reset IP display
    menuWifiIP.name = "IP: --.-.-.-";

    Serial0.println("  WiFi disconnected!");
    menuState.needsRedraw = true;
}

// ============================================
// PAIR TO ESP32 CALLBACKS
// ============================================

void cbPairToEsp32() {
    Serial0.println("Menu: Pair To ESP32 selected");
    Serial0.println("  Select mode:");
    Serial0.println("  - ESP32->WiFi: Connect ESP32 to your WiFi network");
    Serial0.println("  - Device->ESP32: Connect your device to ESP32 AP");
    menuState.needsRedraw = true;
}

void cbEsp32ToWifi() {
    Serial0.println("Menu: ESP32->WiFi mode selected");
    Serial0.println("  Starting WiFi Manager Portal...");
    Serial0.println("  Connect to WiFi and visit: 192.168.4.1");

    if (menuWifiEnabled.boolValue) {
        if (WiFi.getMode() == WIFI_OFF) {
            WiFi.mode(WIFI_STA);
            delay(100);
        }
        menuState.currentMenu = &menuWifiList;
        menuState.selectedItem = menuWifiList.firstChild;
        menuState.needsRedraw = true;
    } else {
        Serial0.println("  WiFi is OFF - enable first");
    }
}

void cbDeviceToEsp32() {
    Serial0.println("Menu: Device->ESP32 mode selected");
    Serial0.println("  Starting ESP32 Access Point...");
    Serial0.println("  SSID: Demi-ESP32");
    Serial0.println("  Password: demiesp32");
    Serial0.println("  IP: 192.168.4.1");

    menuState.needsRedraw = true;
}
