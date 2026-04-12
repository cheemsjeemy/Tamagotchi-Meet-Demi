#include <Arduino.h>
#include "menu.h"
#include "QRCode.h"
#include <string.h>
#include <WiFi.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include "captive_portal.h"
#include <esp_wifi.h>
#include <TOTP.h>
#include <qrcode.h>
#include <time.h>
#include "miscellaneous/names.h"

#include "WiFiHandler.h"


// Forward declarations for TOTP functions
void initTotp();

// Base32 secret for MS Authenticator setup
#define TOTP_BASE32_SECRET "srv62zqnx7hwldq5"


// QR code drawing state
bool qrCodeDrawn = false;
Preferences preferences;

// Connected clients
const int MAX_CLIENTS = 10;
MenuItem menuClients[MAX_CLIENTS];
char clientLabels[MAX_CLIENTS][32];

// Scan failure display timer
static unsigned long scanFailTime = 0;
static char scanStatus[20] = "Scanning...";


// WiFi password input buffer (removed - using captive portal instead)
char wifiPasswordInput[64] = {0};
int wifiPasswordInputLen = 0;
bool isEnteringPassword = false;


// Captive Portal Web Server (Async)
AsyncWebServer captiveServer(80);

// ============================================
// TOTP RELATED VARIABLES
// ============================================

// NTP Configuration for Philippines (UTC+8)
const char* ntpServer = "1.ph.pool.ntp.org";
const long gmtOffset_sec = 28800;
const int daylightOffset_sec = 0;

// TOTP key (will be decoded from base32)
uint8_t hmacKey[20];
int keyLength = 0;

// TOTP instance - will be initialized after key decoding
TOTP* totp = nullptr;

// Current TOTP code (6 digits)
char currentTotpCode[7] = {0};

// Last NTP sync time
unsigned long lastNtpSync = 0;

// Last TOTP code generation time
unsigned long lastTotpUpdate = 0;

// NTP sync interval (1 hour = 3600000 ms)
const unsigned long NTP_SYNC_INTERVAL = 3600000;

// TOTP update interval (30 seconds = 30000 ms)
const unsigned long TOTP_UPDATE_INTERVAL = 30000;

// TOTP display mode flag
bool showTotpCode = false;
bool showTotpSecret = false;

// Global menu state
MenuState menuState = {};
static bool menuSystemInitialized = false;


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
void cbTotpSetup();
void cbFeed();
void cbWifiTest();
void cbBluetoothTest();
void cbSaveSettings();

// WiFi callbacks
void cbWifiStatus();
void cbWifiScan();
void cbWifiForget();
void cbWifiConnect();
void cbWifiDisconnect();
void cbAutoConnectToggle();
void cbShowQR();
void cbShowSSID();
void cbShowPassword();

// Forward declarations for helper functions
void updateWifiStatusText();
void updateConnectionsStatus();
void updateScannedNetworksList();
void updateSavedNetworksList();

// ============================================
// MENU TREE DEFINITION (Using helper functions)
// ============================================
// See menu.h for available helper functions:
//   menuFolder("Name")           - Creates a folder
//   menuAction("Name", callback)  - Creates an action
//   menuToggle("Name", cb, key)   - Creates a toggle
//   menuSlider("Name", val, cb, key) - Creates a slider
//   menuInput("Name", len, key)   - Creates an input
//   menuStatus("Name", callback)  - Creates a status display
// ============================================

// ============================================
// ROOT MENU ITEMS
// ============================================

MenuItem menuSettings = menuFolder("Settings");
MenuItem menuDemi     = menuFolder("Demi");
MenuItem menuWifi     = menuFolder("Wifi");
MenuItem menuBluetooth = menuFolder("Bluetooth");
MenuItem menuMSAuth   = menuFolder("MSAuth");
MenuItem menuPayloads = menuFolder("Payloads");

// ============================================
// SETTINGS SUBMENU
// ============================================

MenuItem menuBrightness    = menuSlider("Brightness", 128, cbBrightness, "brightness");
MenuItem menuAbout         = menuAction("About", cbAbout);
MenuItem menuSaveSettings  = menuAction("Save Settings", cbSaveSettings);

// ============================================
// BLUETOOTH SUBMENU
// ============================================

MenuItem menuBtEnabled   = menuToggle("Enabled", cbBluetoothToggle, "bt_enabled");
MenuItem menuBtSettings  = menuFolder("BT Settings");
MenuItem menuBtName       = menuInput("Device Name", 32, "bt_name");
MenuItem menuBtTest       = menuAction("Test BT", cbBluetoothTest);

// ============================================
// DEMI SUBMENU
// ============================================

MenuItem menuPlay     = menuFolder("Play");
MenuItem menuWash     = menuAction("Wash", cbWash);
MenuItem menuSleep    = menuAction("Sleep", cbSleep);
MenuItem menuFeed      = menuFolder("Feed");

// Play submenu
MenuItem menuPetting  = menuAction("Petting", cbPetting);
MenuItem menuJumping = menuAction("Jumping", cbJumping);
MenuItem menuSing     = menuFolder("Sing");

// Sing submenu - songs from MusicMenu.h
MenuItem menuABCSong    = menuAction("ABC Song", nullptr);
MenuItem menuTheApple   = menuAction("The Apple Code", nullptr);
MenuItem menuNeonDrive  = menuAction("Neon Drive", nullptr);

// Feed -> Fridge submenu (food items)
MenuItem menuFridge     = menuFolder("Fridge");
MenuItem menuApple      = menuAction("Apple", cbFeed);
MenuItem menuBanana     = menuAction("Banana", cbFeed);
MenuItem menuOrange     = menuAction("Orange", cbFeed);
MenuItem menuGrape      = menuAction("Grape", cbFeed);
MenuItem menuStrawberry = menuAction("Strawberry", cbFeed);

// ============================================
// WIFI MENU ITEMS
// ============================================

static char wifiStatusText[20] = "Status: OFF";
MenuItem menuWifiStatus    = menuStatus(wifiStatusText, cbWifiStatus);
MenuItem menuWifiEnabled   = menuToggle("Toggle WiFi", cbWifiToggle, "wifi_enabled");
MenuItem menuWifiDisconnect = menuAction("Disconnect", cbWifiDisconnect);
MenuItem menuConnectToNetwork = menuFolder("Connect to Network");
MenuItem menuScanNetworks  = menuFolder("Scan Networks");
MenuItem menuScanAction    = menuAction("SCAN", cbWifiScan);
MenuItem menuSavedNetworks = menuFolder("Saved Networks");
MenuItem menuWifiScanning  = menuFolder("SCANNING...");
MenuItem menuSavedStatus   = menuStatus("SAVED", nullptr);
MenuItem menuNoItems       = menuStatus("NO ITEMS INSIDE", nullptr);

// Dynamic network items for scanned networks
MenuItem menuWifiNetworks[MAX_WIFI_SCAN_RESULTS];

// Dynamic saved network items
MenuItem menuSavedNetworksList[MAX_SAVED_NETWORKS];
MenuItem menuSavedConnect[MAX_SAVED_NETWORKS] = {
    menuAction("Connect", cbWifiConnect),
    menuAction("Connect", cbWifiConnect),
    menuAction("Connect", cbWifiConnect),
    menuAction("Connect", cbWifiConnect),
    menuAction("Connect", cbWifiConnect)
};
MenuItem menuSavedAutoConnect[MAX_SAVED_NETWORKS] = {
    menuAction("Auto: OFF", cbAutoConnectToggle),
    menuAction("Auto: OFF", cbAutoConnectToggle),
    menuAction("Auto: OFF", cbAutoConnectToggle),
    menuAction("Auto: OFF", cbAutoConnectToggle),
    menuAction("Auto: OFF", cbAutoConnectToggle)
};
MenuItem menuSavedForget[MAX_SAVED_NETWORKS] = {
    menuAction("Forget", cbWifiForget),
    menuAction("Forget", cbWifiForget),
    menuAction("Forget", cbWifiForget),
    menuAction("Forget", cbWifiForget),
    menuAction("Forget", cbWifiForget)
};

// Network submenu items - scanned networks use QR, saved networks use Connect
MenuItem menuNetworkConnect = menuAction("Connect", cbWifiConnect);
MenuItem menuNetworkForget = menuAction("Forget", cbWifiForget);
MenuItem menuNetworkQR      = menuAction("Connect via Phone (QR)", cbShowQR);

// Device Access folder
MenuItem menuDeviceAccess   = menuFolder("Device Access");
MenuItem menuDeviceQR       = menuAction("Show QR", cbShowQR);
MenuItem menuDeviceSSID     = menuAction("SSID: Demi-ESP32", cbShowSSID);
MenuItem menuDevicePassword = menuAction("Pass: demiesp32", cbShowPassword);

// ============================================
// MSAUTH SUBMENU (TOTP)
// ============================================

MenuItem menuTotpSetup      = menuAction("Setup Secret", cbTotpSetup);
MenuItem menuTotpShowCode   = menuAction("Show Code", cbMSAuth);

// ============================================
// CONNECTIONS (NEW - from menu_plan.md)
// ============================================

static char deviceToEsp32Status[24] = "Status: None";
static char deviceToEsp32Name[20] = "Name: Demi-ESP32";
static char deviceToEsp32Ip[24] = "IP: 192.168.4.1";
static char esp32ToNetworkStatus[24] = "Status: Not Connected";
static char esp32ToNetworkSsid[20] = "SSID: (none)";
static char esp32ToNetworkIp[24] = "IP: (none)";

MenuItem menuConnections      = menuFolder("Connections");
MenuItem menuDeviceToEsp32  = menuFolder("Device -> ESP32");
MenuItem menuD2EStatus    = menuStatus(deviceToEsp32Status, nullptr);
MenuItem menuD2EDeviceName = menuStatus(deviceToEsp32Name, nullptr);
MenuItem menuD2EDeviceIp   = menuStatus(deviceToEsp32Ip, nullptr);

MenuItem menuEsp32ToNetwork = menuFolder("ESP32 -> Network");
MenuItem menuE2NStatus     = menuStatus(esp32ToNetworkStatus, nullptr);
MenuItem menuE2NSsid        = menuStatus(esp32ToNetworkSsid, nullptr);
MenuItem menuE2NIp          = menuStatus(esp32ToNetworkIp, nullptr);
MenuItem menuConnectedDevicesList = menuFolder("Connected Devices");

    // ============================================
    // NTP SYNC FUNCTIONS (Now handled by WiFiHandler)
    // ============================================

    bool shouldSyncNtp() {
        return wifiShouldSyncNtp();
    }

    // ============================================
    // TOTP FUNCTIONS
    // ============================================

    // Base32 decoding function for TOTP secret
    int base32Decode(const char* input, uint8_t* output, int maxOutputLen) {
        const char* base32Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";
        int inputLen = strlen(input);
        int outputLen = 0;
        int buffer = 0;
        int bitsLeft = 0;
        
        for (int i = 0; i < inputLen && outputLen < maxOutputLen; i++) {
            char c = toupper(input[i]);
            if (c == '=') break; // Padding character
            
            const char* pos = strchr(base32Chars, c);
            if (!pos) continue; // Invalid character, skip
            
            int value = pos - base32Chars;
            buffer = (buffer << 5) | value;
            bitsLeft += 5;
            
            if (bitsLeft >= 8) {
                output[outputLen++] = (buffer >> (bitsLeft - 8)) & 0xFF;
                bitsLeft -= 8;
            }
        }
        return outputLen;
    }

     // Test TOTP with RFC 6238 test vector
    void testTotp() {
        Serial.println("[TOTP] Testing with RFC 6238 test vector...");
        
        // RFC 6238 test vector:
        // Key: 20 bytes of 0x00 (but we need base32)
        // Time: 59 seconds  
        // Expected: 94287082
        
        // Use a simple test key for debugging
        uint8_t testKey[20] = {0};
        TOTP testTotp(testKey, 20);
        
        char* testCode = testTotp.getCode(59); // Time = 59 seconds
        Serial.print("[TOTP] Test code at time 59: ");
        Serial.println(testCode != nullptr ? testCode : "NULL");
        
        if (testCode && strcmp(testCode, "94287082") == 0) {
            Serial.println("[TOTP] RFC 6238 test PASSED");
        } else {
            Serial.println("[TOTP] RFC 6238 test FAILED");
        }
    }
    
    // Initialize TOTP with base32 secret
    void initTotp() {
        if (totp != nullptr) {
            delete totp;
            totp = nullptr;
        }

        // Decode the Base32 secret key
        keyLength = base32Decode(TOTP_BASE32_SECRET, hmacKey, sizeof(hmacKey));

        if (keyLength > 0) {
            Serial.print("[TOTP] Decoded key length: ");
            Serial.println(keyLength);
            Serial.print("[TOTP] Key bytes: ");
            for (int i = 0; i < keyLength; i++) {
                Serial.printf("0x%02X, ", hmacKey[i]);
            }
            Serial.println();

            // Initialize the TOTP instance
            totp = new TOTP(hmacKey, keyLength);
            Serial.println("[TOTP] TOTP initialized successfully");

            // Test TOTP library with known vector
            testTotp();
        } else {
            Serial.println("[TOTP] Failed to decode Base32 secret key");
        }
    }

    // Check if TOTP is properly initialized
    bool hasTotpKey() {
        return totp != nullptr && keyLength > 0;
    }

    // Get the base32 secret for MS Authenticator setup
    const char* getTotpSecret() {
        return TOTP_BASE32_SECRET;
    }

    // Generate and display the TOTP code
    void generateTotpCode() {
        if (!hasTotpKey()) {
            Serial.println("[TOTP] TOTP is not initialized");
            return;
        }

        // Get the current time
        time_t now = time(nullptr);
        if (now < 1000000000) { // Ensure time is synced
            Serial.println("[TOTP] Time not synced yet");
            
            return;
        }

        // Generate the TOTP code
        char* code = totp->getCode(now);
        if (code != nullptr) {
            strncpy(currentTotpCode, code, sizeof(currentTotpCode) - 1);
            currentTotpCode[sizeof(currentTotpCode) - 1] = '\0';
            //Serial.print("[TOTP] Generated code: ");
            //Serial.println(currentTotpCode);
        } else {
            Serial.println("[TOTP] Failed to generate code");
        }
    }

    bool shouldUpdateTotp() {
        return (millis() - lastTotpUpdate) >= TOTP_UPDATE_INTERVAL;
    }

    // ============================================
    // PREFERENCES FUNCTIONS
    // ============================================

    void initPreferences() {
        preferences.begin("demi settings", false);
        Serial.println("Preferences initialized");
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

        Serial.println("All settings saved to preferences");
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

        // Load Bluetooth Name
        if (preferences.isKey("bt_name")) {
            String btName = preferences.getString("bt_name", "Demi-ESP32");
            if (menuBtName.stringValue == nullptr) {
                menuBtName.stringValue = new char[33];
            }
            strncpy(menuBtName.stringValue, btName.c_str(), 32);
            menuBtName.stringValue[32] = '\0';
        }

        Serial.println("Settings loaded from preferences");
    }



    void updateSavedNetworksList() {
        static char savedDisplayNames[MAX_SAVED_NETWORKS][40];
        static char autoLabels[MAX_SAVED_NETWORKS][20];  // Fixed: one buffer per network
        
        // Link saved networks as children of Saved Networks
        if (numSavedNetworks == 0) {
            menuSavedNetworks.firstChild = nullptr;
            return;
        }
        
        for (int i = 0; i < numSavedNetworks && i < MAX_SAVED_NETWORKS; i++) {
            // Handle duplicate SSIDs by adding suffix
            int dupeCount = 1;
            for (int j = 0; j < i; j++) {
                if (strcmp(savedNetworks[j].ssid, savedNetworks[i].ssid) == 0) {
                    dupeCount++;
                }
            }
            if (dupeCount > 1) {
                snprintf(savedDisplayNames[i], sizeof(savedDisplayNames[i]), "%s %d", savedNetworks[i].ssid, dupeCount);
            } else {
                strncpy(savedDisplayNames[i], savedNetworks[i].ssid, 39);
                savedDisplayNames[i][39] = '\0';
            }
            menuSavedNetworksList[i].name = savedDisplayNames[i];
            menuSavedNetworksList[i].parent = &menuSavedNetworks;
            
            // Link each saved network to dedicated Connect, Auto, and Forget actions
            menuSavedNetworksList[i].firstChild = &menuSavedConnect[i];
            menuSavedConnect[i].parent = &menuSavedNetworksList[i];
            menuSavedConnect[i].prevSibling = nullptr;
            menuSavedConnect[i].nextSibling = &menuSavedAutoConnect[i];
            menuSavedAutoConnect[i].prevSibling = &menuSavedConnect[i];
            menuSavedAutoConnect[i].parent = &menuSavedNetworksList[i];
            menuSavedAutoConnect[i].nextSibling = &menuSavedForget[i];
            menuSavedForget[i].prevSibling = &menuSavedAutoConnect[i];
            menuSavedForget[i].parent = &menuSavedNetworksList[i];
            menuSavedForget[i].nextSibling = nullptr;
            
            // Update auto-connect toggle display (use separate buffer per index)
            snprintf(autoLabels[i], sizeof(autoLabels[i]), "Auto: %s", savedNetworks[i].autoConnect ? "ON" : "OFF");
            menuSavedAutoConnect[i].name = autoLabels[i];
            
            if (i == 0) {
                menuSavedNetworks.firstChild = &menuSavedNetworksList[i];
                menuSavedNetworksList[i].prevSibling = nullptr;
            } else {
                menuSavedNetworksList[i].prevSibling = &menuSavedNetworksList[i - 1];
                menuSavedNetworksList[i - 1].nextSibling = &menuSavedNetworksList[i];
            }
            
            if (i == numSavedNetworks - 1) {
                menuSavedNetworksList[i].nextSibling = nullptr;
            }
        }
    }

    void connectToWifiNetwork(int index) {
        if (index < 0 || index >= numSavedNetworks) {
            Serial.println("[WiFi] Invalid network index");
            return;
        }

        Serial.print("[WiFi] Connecting to saved network: ");
        Serial.println(savedNetworks[index].ssid);

        // Use non-blocking async connect
        wifiConnectAsync(savedNetworks[index].ssid, savedNetworks[index].password);
    }

    // ============================================
    // LINK MENU ITEMS TOGETHER
    // ============================================

    void linkMenuItems() {
        // Link root items together as siblings
        menuSettings.nextSibling = &menuDemi;
        menuDemi.prevSibling = &menuSettings;
        menuDemi.nextSibling = &menuMSAuth;
        menuMSAuth.prevSibling = &menuDemi;
        menuMSAuth.nextSibling = &menuPayloads;
        menuPayloads.prevSibling = &menuMSAuth;
        menuPayloads.nextSibling = nullptr;

        // Settings children: WiFi -> Bluetooth -> Brightness -> About -> Save Settings
        menuSettings.firstChild = &menuWifi;

        // WiFi
        menuWifi.parent = &menuSettings;
        menuWifi.prevSibling = nullptr;  // first child
        menuWifi.nextSibling = &menuBluetooth;

        // Bluetooth
        menuBluetooth.parent = &menuSettings;
        menuBluetooth.prevSibling = &menuWifi;
        menuBluetooth.nextSibling = &menuBrightness;

        // Bluetooth children: Enabled -> Device Name -> Test BT
        menuBluetooth.firstChild = &menuBtEnabled;
        menuBtEnabled.parent = &menuBluetooth;
        menuBtEnabled.prevSibling = nullptr;
        menuBtEnabled.nextSibling = &menuBtName;

        menuBtName.parent = &menuBluetooth;
        menuBtName.prevSibling = &menuBtEnabled;
        menuBtName.nextSibling = &menuBtTest;

        menuBtTest.parent = &menuBluetooth;
        menuBtTest.prevSibling = &menuBtName;
        menuBtTest.nextSibling = nullptr;

        // Brightness
        menuBrightness.parent = &menuSettings;
        menuBrightness.prevSibling = &menuBluetooth;
        menuBrightness.nextSibling = &menuAbout;
        menuAbout.prevSibling = &menuBrightness;
        menuAbout.nextSibling = &menuSaveSettings;
        menuSaveSettings.prevSibling = &menuAbout;
        menuSaveSettings.nextSibling = nullptr;

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

        // Play children: Petting -> Jumping -> Sing
        menuPlay.firstChild = &menuPetting;
        menuPetting.parent = &menuPlay;
        menuPetting.nextSibling = &menuJumping;
        menuJumping.prevSibling = &menuPetting;
        menuJumping.parent = &menuPlay;
        menuJumping.nextSibling = &menuSing;
        menuSing.prevSibling = &menuJumping;
        menuSing.parent = &menuPlay;
        menuSing.nextSibling = nullptr;

        // Sing children: ABC Song -> The Apple Code -> Neon Drive
        menuSing.firstChild = &menuABCSong;
        menuABCSong.parent = &menuSing;
        menuABCSong.nextSibling = &menuTheApple;
        menuTheApple.prevSibling = &menuABCSong;
        menuTheApple.parent = &menuSing;
        menuTheApple.nextSibling = &menuNeonDrive;
        menuNeonDrive.prevSibling = &menuTheApple;
        menuNeonDrive.parent = &menuSing;
        menuNeonDrive.nextSibling = nullptr;

        // Feed -> Fridge children: Fridge
        menuFeed.firstChild = &menuFridge;
        menuFridge.parent = &menuFeed;
        menuFridge.nextSibling = nullptr;

        // Fridge children (food items): Apple -> Banana -> Orange -> Grape -> Strawberry
        menuFridge.firstChild = &menuApple;
        menuApple.parent = &menuFridge;
        menuApple.nextSibling = &menuBanana;
        menuBanana.prevSibling = &menuApple;
        menuBanana.parent = &menuFridge;
        menuBanana.nextSibling = &menuOrange;
        menuOrange.prevSibling = &menuBanana;
        menuOrange.parent = &menuFridge;
        menuOrange.nextSibling = &menuGrape;
        menuGrape.prevSibling = &menuOrange;
        menuGrape.parent = &menuFridge;
        menuGrape.nextSibling = &menuStrawberry;
        menuStrawberry.prevSibling = &menuGrape;
        menuStrawberry.parent = &menuFridge;
        menuStrawberry.nextSibling = nullptr;

        // WiFi children (new structure per menu_plan.md):
        // Connections -> Toggle WiFi -> Connect to Network -> Device Access
        // WiFi children: Toggle WiFi -> Connections -> Connect to Network -> Device Access
        menuWifi.firstChild = &menuWifiEnabled;

        // WiFi Toggle
        menuWifiEnabled.parent = &menuWifi;
        menuWifiEnabled.prevSibling = nullptr;  // first child
        menuWifiEnabled.nextSibling = &menuWifiDisconnect;

        // WiFi Disconnect
        menuWifiDisconnect.parent = &menuWifi;
        menuWifiDisconnect.prevSibling = &menuWifiEnabled;
        menuWifiDisconnect.nextSibling = &menuConnections;

        // Connections
        menuConnections.parent = &menuWifi;
        menuConnections.prevSibling = &menuWifiDisconnect;
        menuConnections.nextSibling = &menuConnectToNetwork;

        // Connect to Network
        menuConnectToNetwork.parent = &menuWifi;
        menuConnectToNetwork.prevSibling = &menuConnections;
        menuConnectToNetwork.nextSibling = &menuDeviceAccess;

        // Device Access
        menuDeviceAccess.parent = &menuWifi;
        menuDeviceAccess.prevSibling = &menuConnectToNetwork;
        menuDeviceAccess.nextSibling = nullptr;

        // Connections children: Device->ESP32 -> ESP32->Network
        menuConnections.firstChild = &menuDeviceToEsp32;
        menuDeviceToEsp32.parent = &menuConnections;
        menuDeviceToEsp32.nextSibling = &menuEsp32ToNetwork;
        menuEsp32ToNetwork.prevSibling = &menuDeviceToEsp32;
        menuEsp32ToNetwork.parent = &menuConnections;
        menuEsp32ToNetwork.nextSibling = nullptr;

        // Device->ESP32 children: Status -> Device Name -> Device IP -> Connected Devices
        menuDeviceToEsp32.firstChild = &menuD2EStatus;
        menuD2EStatus.parent = &menuDeviceToEsp32;
        menuD2EStatus.nextSibling = &menuD2EDeviceName;
        menuD2EDeviceName.prevSibling = &menuD2EStatus;
        menuD2EDeviceName.parent = &menuDeviceToEsp32;
        menuD2EDeviceName.nextSibling = &menuD2EDeviceIp;
        menuD2EDeviceIp.prevSibling = &menuD2EDeviceName;
        menuD2EDeviceIp.parent = &menuDeviceToEsp32;
        menuD2EDeviceIp.nextSibling = &menuConnectedDevicesList;
        menuConnectedDevicesList.prevSibling = &menuD2EDeviceIp;
        menuConnectedDevicesList.parent = &menuDeviceToEsp32;
        menuConnectedDevicesList.nextSibling = nullptr;

        // ESP32->Network children: Status -> SSID -> IP
        menuEsp32ToNetwork.firstChild = &menuE2NStatus;
        menuE2NStatus.parent = &menuEsp32ToNetwork;
        menuE2NStatus.nextSibling = &menuE2NSsid;
        menuE2NSsid.prevSibling = &menuE2NStatus;
        menuE2NSsid.parent = &menuEsp32ToNetwork;
        menuE2NSsid.nextSibling = &menuE2NIp;
        menuE2NIp.prevSibling = &menuE2NSsid;
        menuE2NIp.parent = &menuEsp32ToNetwork;
        menuE2NIp.nextSibling = nullptr;

        // Connect to Network children: Scan Networks -> Saved Networks
        menuConnectToNetwork.firstChild = &menuScanNetworks;
        menuScanNetworks.parent = &menuConnectToNetwork;
        menuScanNetworks.nextSibling = &menuSavedNetworks;
        menuSavedNetworks.prevSibling = &menuScanNetworks;
        menuSavedNetworks.parent = &menuConnectToNetwork;
        menuSavedNetworks.nextSibling = nullptr;

        // Saved Networks children: (populated dynamically when networks are saved)
        menuSavedNetworks.firstChild = nullptr;

        // Scan Networks children: SCAN action + dynamic network list
        menuScanNetworks.firstChild = &menuScanAction;
        menuScanAction.parent = &menuScanNetworks;
        menuScanAction.nextSibling = nullptr;

        // Device Access children: Show QR -> SSID -> Password
        menuDeviceAccess.firstChild = &menuDeviceQR;
        menuDeviceQR.parent = &menuDeviceAccess;
        menuDeviceQR.nextSibling = &menuDeviceSSID;
        menuDeviceSSID.prevSibling = &menuDeviceQR;
        menuDeviceSSID.parent = &menuDeviceAccess;
        menuDeviceSSID.nextSibling = &menuDevicePassword;
        menuDevicePassword.prevSibling = &menuDeviceSSID;
        menuDevicePassword.parent = &menuDeviceAccess;
        menuDevicePassword.nextSibling = nullptr;

        // Initialize scanned network items
        for (int i = 0; i < MAX_WIFI_SCAN_RESULTS; i++) {
            menuWifiNetworks[i].type = MENU_FOLDER;
            menuWifiNetworks[i].name = "";
            menuWifiNetworks[i].firstChild = &menuNetworkConnect;
            menuWifiNetworks[i].parent = nullptr;
            menuWifiNetworks[i].nextSibling = nullptr;
            menuWifiNetworks[i].prevSibling = nullptr;
        }

        // Initialize saved network items
        for (int i = 0; i < MAX_SAVED_NETWORKS; i++) {
            menuSavedNetworksList[i].type = MENU_FOLDER;
            menuSavedNetworksList[i].name = "";
            menuSavedNetworksList[i].firstChild = nullptr; // safer than pointing to single global template
            menuSavedNetworksList[i].parent = &menuSavedNetworks;
            menuSavedNetworksList[i].nextSibling = nullptr;
            menuSavedNetworksList[i].prevSibling = nullptr;

            menuSavedConnect[i].parent = nullptr;
            menuSavedConnect[i].firstChild = nullptr;
            menuSavedConnect[i].prevSibling = nullptr;
            menuSavedConnect[i].nextSibling = nullptr;

            menuSavedAutoConnect[i].parent = nullptr;
            menuSavedAutoConnect[i].firstChild = nullptr;
            menuSavedAutoConnect[i].prevSibling = nullptr;
            menuSavedAutoConnect[i].nextSibling = nullptr;

            menuSavedForget[i].parent = nullptr;
            menuSavedForget[i].firstChild = nullptr;
            menuSavedForget[i].prevSibling = nullptr;
            menuSavedForget[i].nextSibling = nullptr;
        }
        // Network submenu: Connect -> Forget -> QR (for scanned networks)
        menuNetworkConnect.parent = nullptr;
        menuNetworkConnect.nextSibling = &menuNetworkForget;
        menuNetworkForget.prevSibling = &menuNetworkConnect;
        menuNetworkForget.parent = nullptr;
        menuNetworkForget.nextSibling = &menuNetworkQR;
        menuNetworkQR.prevSibling = &menuNetworkForget;
        menuNetworkQR.parent = nullptr;
        menuNetworkQR.nextSibling = nullptr;

        // MSAuth submenu: Setup Secret -> Show Code
        menuMSAuth.firstChild = &menuTotpSetup;
        menuTotpSetup.parent = &menuMSAuth;
        menuTotpSetup.nextSibling = &menuTotpShowCode;
        menuTotpShowCode.parent = &menuMSAuth;
        menuTotpShowCode.nextSibling = nullptr;
        menuTotpShowCode.prevSibling = &menuTotpSetup;
    }

    // ============================================
    // MENU STATE FUNCTIONS
    // ============================================

    void initMenu() {
        if (!menuSystemInitialized) {
            linkMenuItems();

            // Initialize preferences and load saved settings once.
            initPreferences();
            loadAllSettings();
            updateSavedNetworksList();

            initTotp();

            if (hasTotpKey()) {
                Serial.println("[initMenu] TOTP initialized with hardcoded secret");
                Serial.print("[initMenu] TOTP key bytes: ");
                for (int i = 0; i < keyLength; i++) {
                    if (i > 0) Serial.print(", ");
                    Serial.print("0x");
                    Serial.print(hmacKey[i], HEX);
                }
                Serial.println();
                generateTotpCode();
            } else {
                Serial.println("[initMenu] Warning: TOTP hmacKey is all zeros");
            }

            menuSystemInitialized = true;
        }

        wifiReloadSavedNetworks();
        updateSavedNetworksList();

        // Start at root for each menu entry, but don't reinitialize services.
        menuState.currentMenu = nullptr;
        menuState.selectedItem = &menuSettings;
        menuState.scrollOffset = 0;
        menuState.needsRedraw = true;
        menuState.isEditing = false;
        menuState.justEntered = false;
    }


    void updateConnectionsStatus() {
        if (wifiIsApActive()) {
            snprintf(deviceToEsp32Status, sizeof(deviceToEsp32Status), "Status: Active");
            snprintf(deviceToEsp32Name, sizeof(deviceToEsp32Name), "Name: %s", wifiGetApSSID().c_str());
            snprintf(deviceToEsp32Ip, sizeof(deviceToEsp32Ip), "IP: %s", wifiGetApIP().c_str());
        } else {
            snprintf(deviceToEsp32Status, sizeof(deviceToEsp32Status), "Status: None");
            snprintf(deviceToEsp32Name, sizeof(deviceToEsp32Name), "Name: (none)");
            snprintf(deviceToEsp32Ip, sizeof(deviceToEsp32Ip), "IP: (none)");
        }

        // Use WiFiHandler status
        if (wifiIsConnected()) {
            snprintf(esp32ToNetworkStatus, sizeof(esp32ToNetworkStatus), "Status: Connected");
            snprintf(esp32ToNetworkSsid, sizeof(esp32ToNetworkSsid), "SSID: %s", wifiGetCurrentSSID().c_str());
            snprintf(esp32ToNetworkIp, sizeof(esp32ToNetworkIp), "IP: %s", wifiGetLocalIP().toString().c_str());
        } else {
            snprintf(esp32ToNetworkStatus, sizeof(esp32ToNetworkStatus), "Status: Not Connected");
            snprintf(esp32ToNetworkSsid, sizeof(esp32ToNetworkSsid), "SSID: (none)");
            snprintf(esp32ToNetworkIp, sizeof(esp32ToNetworkIp), "IP: (none)");
        }

        // Update connected devices list
        wifi_sta_list_t sta_list;
        esp_err_t err = esp_wifi_ap_get_sta_list(&sta_list);
        int clientCount = 0;
        if (err == ESP_OK && sta_list.num > 0) {
            for (int i = 0; i < sta_list.num && clientCount < MAX_CLIENTS; i++) {
                wifi_sta_info_t sta = sta_list.sta[i];

                String playerName = generate(sta.mac);

                sprintf(clientLabels[clientCount], "P%d: %s", clientCount + 1, playerName.c_str());
                menuClients[clientCount] = menuStatus(clientLabels[clientCount], nullptr);
                menuClients[clientCount].parent = &menuConnectedDevicesList;
                if (clientCount == 0) {
                    menuConnectedDevicesList.firstChild = &menuClients[clientCount];
                    menuClients[clientCount].prevSibling = nullptr;
                } else {
                    menuClients[clientCount].prevSibling = &menuClients[clientCount - 1];
                    menuClients[clientCount - 1].nextSibling = &menuClients[clientCount];
                }
                clientCount++;
            }
        }
        if (clientCount > 0) {
            menuClients[clientCount - 1].nextSibling = nullptr;
        } else {
            menuConnectedDevicesList.firstChild = &menuNoItems;
        }
    }



    MenuItem* getFirstItem(MenuItem* menu) {
        if (menu == nullptr) {
            return &menuSettings;
        }

        // Update WiFi status text based on current state
        if (menu == &menuWifi) {
            updateWifiStatusText();
        }

        // Update Connections status (Device->ESP32 and ESP32->Network)
        if (menu == &menuConnections) {
            updateConnectionsStatus();
        }


        
        // Update saved networks list when entering Saved Networks
        if (menu == &menuSavedNetworks) {
            wifiReloadSavedNetworks();
            updateSavedNetworksList();
        }

        // Update scanned networks list whenever entering Scan Networks
        if (menu == &menuScanNetworks) {
            updateScannedNetworksList();
        }

        if (menu->firstChild == nullptr) {
            return &menuNoItems;
        }
        return menu->firstChild;
    }

    void updateWifiStatusText() {
        if (!menuWifiEnabled.boolValue) {
            snprintf(wifiStatusText, sizeof(wifiStatusText), "Status: OFF");
        } else if (wifiIsConnected()) {
            snprintf(wifiStatusText, sizeof(wifiStatusText), "Status: CONNECTED");
        } else {
            snprintf(wifiStatusText, sizeof(wifiStatusText), "Status: NOT CONNECTED");
        }
    }


    void updateScannedNetworksList() {
        // Link scanned networks as siblings after SCAN action
        for (int i = 0; i < numScanResults && i < MAX_WIFI_SCAN_RESULTS; i++) {
            static char scannedNames[MAX_WIFI_SCAN_RESULTS][33];
            
            // Handle duplicate SSIDs by adding suffix
            int dupeCount = 1;
            for (int j = 0; j < i; j++) {
                if (strcmp(scanResults[j].ssid, scanResults[i].ssid) == 0) {
                    dupeCount++;
                }
            }
            if (dupeCount > 1) {
                snprintf(scannedNames[i], sizeof(scannedNames[i]), "%s %d", scanResults[i].ssid, dupeCount);
            } else {
                snprintf(scannedNames[i], sizeof(scannedNames[i]), "%s", scanResults[i].ssid);
            }
            
            menuWifiNetworks[i].name = scannedNames[i];
            menuWifiNetworks[i].parent = &menuScanNetworks;

            // Link each scanned network to QR action and SAVED if saved
            menuWifiNetworks[i].firstChild = &menuNetworkQR;
            menuNetworkQR.parent = &menuWifiNetworks[i];
            menuNetworkQR.prevSibling = nullptr;
            if (isWifiNetworkSaved(scanResults[i].ssid)) {
                menuWifiNetworks[i].nextSibling = &menuSavedStatus;
                menuSavedStatus.parent = &menuWifiNetworks[i];
                menuSavedStatus.nextSibling = nullptr;
            } else {
                menuWifiNetworks[i].nextSibling = nullptr;
            }

            if (i == 0) {
                menuScanAction.nextSibling = &menuWifiNetworks[i];
                menuWifiNetworks[i].prevSibling = &menuScanAction;
            } else {
                menuWifiNetworks[i].prevSibling = &menuWifiNetworks[i - 1];
                menuWifiNetworks[i - 1].nextSibling = &menuWifiNetworks[i];
            }

            if (i == numScanResults - 1) {
                menuWifiNetworks[i].nextSibling = nullptr;
            }
        }

        // If no networks, ensure SCAN has no siblings
        if (numScanResults == 0) {
            menuScanAction.nextSibling = nullptr;
        }
    }

    MenuItem* getNextVisibleItem(MenuItem* item) {
        if (!item) return nullptr;

        // Special handling for WiFi folder - hide children when disabled
        if (item->parent == &menuWifi && !menuWifiEnabled.boolValue) {
            // WiFi disabled - don't show any siblings after Enabled
            return nullptr;
        }

        return item->nextSibling;
    }

    MenuItem* getPrevVisibleItem(MenuItem* item) {
        if (!item) return nullptr;

        // Special handling for WiFi folder - hide children when disabled
        if (item->parent == &menuWifi && !menuWifiEnabled.boolValue) {
            // WiFi disabled - don't show any siblings before Enabled
            return nullptr;
        }

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
        
        // Reset network selection and QR flags when going back
        if (isInScannedNetwork || isInSavedNetwork) {
            isInScannedNetwork = false;
            isInSavedNetwork = false;
            selectedNetworkIndex = -1;
        }
        
        // Reset QR codes when going back
        if (menuState.showQRCode) {
            menuState.showQRCode = false;
            qrCodeDrawn = false;
        }
        if (showTotpCode) {
            showTotpCode = false;
        }
        if (showTotpSecret) {
            showTotpSecret = false;
        }
        
        beepC5(80);
    }

    void resetQRCodeState() {
        qrCodeDrawn = false;
    }



    void menuEnter() {
        // If showing QR code, dismiss it and go back to menu
        if (menuState.showQRCode) {
            menuState.showQRCode = false;
            menuState.needsRedraw = true;
            qrCodeDrawn = false;  // Reset flag so QR can be drawn again next time
            Serial.println("[Menu] Dismissed QR code");
            return;
        }

        // If showing TOTP code, dismiss it and go back to menu
        if (showTotpCode) {
            showTotpCode = false;
            menuState.needsRedraw = true;
            Serial.println("[Menu] Dismissed TOTP code display");
            return;
        }

        // If showing TOTP secret, dismiss it and go back to menu
        if (showTotpSecret) {
            showTotpSecret = false;
            menuState.needsRedraw = true;
            Serial.println("[Menu] Dismissed TOTP secret display");
            return;
        }

        // Can't enter/execute when editing
        if (menuState.isEditing) {
            menuState.isEditing = false;
            menuState.needsRedraw = true;
            saveAllSettings();
            return;
        }
        if (!menuState.selectedItem) return;

        if (menuState.selectedItem->type == MENU_FOLDER) {
            // Do not auto-scan when entering WiFi menu

            // Check if entering a scanned network folder
            isInScannedNetwork = false;
            isInSavedNetwork = false;
            for (int i = 0; i < numScanResults; i++) {
                if (menuState.selectedItem == &menuWifiNetworks[i]) {
                    selectedNetworkIndex = i;
                    isInScannedNetwork = true;
                    Serial.print("[Menu] Entered scanned network: ");
                    Serial.println(scanResults[i].ssid);
                    break;
                }
            }

            // Check if entering a saved network folder
            if (!isInScannedNetwork) {
                for (int i = 0; i < numSavedNetworks; i++) {
                    if (menuState.selectedItem == &menuSavedNetworksList[i]) {
                        selectedNetworkIndex = i;
                        isInSavedNetwork = true;
                        Serial.print("[Menu] Entered saved network: ");
                        Serial.println(savedNetworks[i].ssid);
                        break;
                    }
                }
            }

            menuState.lastSelectedItem = menuState.selectedItem;
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
        } else if (menuState.selectedItem->type == MENU_INPUT) {
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
        return digitalRead(pin) == LOW;
    }

    bool shouldEnterMenu() {
        return false;
    }

    bool shouldExitMenu() {
        if (isKeyPressed(BTN_LB) && isKeyPressed(BTN_RB)) {
            return true;
        }
        return false;
    }

    bool shouldGoBack() {
        static unsigned long lastLeftPress = 0;
        static bool leftWasPressed = false;

        bool leftPressed = isKeyPressed(BTN_LEFT);
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

    // Display QR code on OLED (WiFi connection QR - 128x64 bitmap)
    // Only draws once using global flag (qrCodeDrawn defined at line 10)
    void displayQRCode(U8G2& u8g2) {
        if (!qrCodeDrawn) {
            u8g2.clearBuffer();
            u8g2.drawBitmap(0, 0, 16, 64, WIFI_QR);
            u8g2.sendBuffer();
            qrCodeDrawn = true;
            Serial.println("[QRCode] Drew WiFi QR code (128x64)");
        }
    }

    // Reset QR code draw flag (call when dismissing QR)



    void renderMenu(U8G2& u8g2) {

        if (menuState.showQRCode) {
            displayQRCode(u8g2);
            return;
        }

        if (showTotpSecret) {
            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.setDrawColor(1);

            u8g2.drawStr(0, 10, "MS Authenticator");
            u8g2.drawStr(0, 25, "Setup Secret:");
            
            // Display the base32 secret
            const char* secret = getTotpSecret();
            u8g2.setFont(u8g2_font_7x14_tf);
            u8g2.drawStr(0, 50, secret);
            
            u8g2.setFont(u8g2_font_5x8_tr);
            u8g2.drawStr(0, 70, "Enter this in MS Authenticator:");
            u8g2.drawStr(0, 80, "1. Open MS Authenticator");
            u8g2.drawStr(0, 90, "2. Add account -> Other");
            u8g2.drawStr(0, 100, "3. Enter secret above");
            u8g2.drawStr(0, 110, "4. Account name: Demi");
            
            u8g2.sendBuffer();
            return;
        }

        if (showTotpCode) {
            time_t now;

            // Check if WiFi is still connected
            if (!wifiIsConnected()) {
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_6x10_tf);
                u8g2.setDrawColor(1);

                u8g2.drawStr(0, 10, "WiFi Required!");
                u8g2.drawStr(0, 25, "Connect to WiFi");
                u8g2.drawStr(0, 40, "via Wifi menu");
                u8g2.drawStr(0, 55, "before generating");
                u8g2.drawStr(0, 70, "TOTP codes");

                u8g2.sendBuffer();
                menuState.needsRedraw = true;
                return;
            }

            // Try to sync NTP if not synced
            if (shouldSyncNtp() && wifiIsConnected()) {
                // NTP sync handled by WiFiHandler
            }

            // Generate code only if time is synced
            now = time(nullptr);
            if (now < 1000000000) {
                u8g2.clearBuffer();
                u8g2.setFont(u8g2_font_6x10_tf);
                u8g2.setDrawColor(1);
                u8g2.drawStr(10, 32, "Syncing time...");
                u8g2.sendBuffer();
                menuState.needsRedraw = true;
                return;
            }

            // Update TOTP code if needed
            if (shouldUpdateTotp()) {
                generateTotpCode();
            }

            u8g2.clearBuffer();
            u8g2.setFont(u8g2_font_6x10_tf);
            u8g2.setDrawColor(1);

            // Draw glyphs in top right corner (126x64 display)
            u8g2.setFont(u8g2_font_siji_t_6x10);
            if (wifiIsConnected()) {
                u8g2.drawGlyph(100, 10, 0xE21A);  // WiFi icon
            }
            if (lastNtpSync > 0 && (millis() - lastNtpSync) < NTP_SYNC_INTERVAL) {
                u8g2.drawGlyph(116, 10, 0xE015);  // Clock icon
            }
            u8g2.setFont(u8g2_font_6x10_tf);

            // Draw TOTP code centered with spaces (XXX XXX)
            char displayCode[8];
            if (strlen(currentTotpCode) == 6) {
                snprintf(displayCode, sizeof(displayCode), "%c%c%c %c%c%c",
                        currentTotpCode[0], currentTotpCode[1], currentTotpCode[2],
                        currentTotpCode[3], currentTotpCode[4], currentTotpCode[5]);
            } else {
                strcpy(displayCode, currentTotpCode);
            }
            
            // Center the code on 126px width: (126-59)/2 = 33
            u8g2.setFont(u8g2_font_10x20_tf);
            u8g2.drawStr(33, 28, displayCode);

            // Draw PH and UTC time below the code
            struct tm localTimeInfo;
            struct tm utcTimeInfo;
            localtime_r(&now, &localTimeInfo);
            gmtime_r(&now, &utcTimeInfo);

            char phClock[16];
            char utcClock[16];
            int localHour = localTimeInfo.tm_hour % 12;
            if (localHour == 0) localHour = 12;
            snprintf(phClock, sizeof(phClock), "PH:%02d:%02d",
                     localHour, localTimeInfo.tm_min);
            
            int utcHour = utcTimeInfo.tm_hour % 12;
            if (utcHour == 0) utcHour = 12;
            snprintf(utcClock, sizeof(utcClock), "UTC:%02d:%02d",
                     utcHour, utcTimeInfo.tm_min);
            
            u8g2.setFont(u8g2_font_5x8_tr);
            u8g2.drawStr(13, 38, phClock);
            u8g2.drawStr(66, 38, utcClock);

            // Draw countdown progress bar at bottom (126px)
            unsigned long elapsed = (millis() - lastTotpUpdate) % TOTP_UPDATE_INTERVAL;
            uint8_t progress = (elapsed * 126) / TOTP_UPDATE_INTERVAL;
            u8g2.drawFrame(0, 54, 126, 4);
            u8g2.drawBox(0, 54, progress, 4);

            // Draw seconds remaining
            uint8_t secondsLeft = (TOTP_UPDATE_INTERVAL - elapsed) / 1000;
            char timerStr[3];
            snprintf(timerStr, sizeof(timerStr), "%d", secondsLeft);
            u8g2.setFont(u8g2_font_5x8_tr);
            u8g2.drawStr(110, 52, timerStr);

            u8g2.sendBuffer();
            menuState.needsRedraw = true;  // Keep redrawing for animation
            return;
        }
        
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
            strcpy(scanStatus, "Scanning");
            // Animated scanning text
            static const char* scanDots[] = {".", "..", "..."};
            uint8_t dotIndex = (millis() / 500) % 3;
            char scanText[20];
            snprintf(scanText, sizeof(scanText), "%s%s", scanStatus, scanDots[dotIndex]);
            u8g2.drawStr(0, 8, scanText);
        } else if (numScanResults == 0) {
            strncpy(scanStatus, "No WiFi sources nearby", sizeof(scanStatus)-1);
            scanStatus[sizeof(scanStatus)-1] = '\0';
            scanFailTime = millis();
            u8g2.drawStr(0, 8, scanStatus);
        } else {
            scanFailTime = 0;
        }

        if (isWifiScanning || numScanResults == 0) {
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
            if (strlen(pathBuf) > 25) {
                char *lastSlash = strrchr(pathBuf, '/'); // Find the file name
                if (lastSlash != nullptr) {
                    char *secondLastSlash = nullptr;
                    // Look for the folder name before the file
                    for (char *p = lastSlash - 1; p >= pathBuf; p--) {
                        if (*p == '/') {
                            secondLastSlash = p;
                            break;
                        }
                    }

                    char shortPath[30] = "../"; 
                    if (secondLastSlash != nullptr) {
                        // Found a parent folder! Result: ../Folder/File.h
                        strncat(shortPath, secondLastSlash + 1, 21); 
                    } else {
                        // Only the file is deep. Result: ../File.h
                        strncat(shortPath, lastSlash + 1, 21);
                    }

                    strncpy(pathBuf, shortPath, 25);
                    pathBuf[25] = '\0'; // Force safety stop
                }
            }


            u8g2.drawStr(0, 8, pathBuf);
            u8g2.drawLine(0, 12, 127, 12);
        }

        // Handle scan failure timeout
        if (scanFailTime > 0 && millis() - scanFailTime > 3000) {
            isWifiScanning = false;
            scanFailTime = 0;
            menuState.needsRedraw = true;
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

        // Keep redrawing during WiFi scan for progressive updates
        if (isWifiScanning) {
            menuState.needsRedraw = true;
        }
    }

    // ============================================
    // MENU STATE FUNCTIONS
    // ============================================

    void cbAbout() {
        Serial.println("Menu: About selected");
    }

    void cbPetting() {
        Serial.println("Menu: Petting action!");
    }

    void cbJumping() {
        Serial.println("Menu: Jumping action!");
    }

    void cbWash() {
        Serial.println("Menu: Wash action!");
    }

    void cbSleep() {
        Serial.println("Menu: Sleep action!");
    }

    void cbFeed() {
        Serial.println("Menu: Feed action!");
    }

    void cbWifiToggle() {
        menuWifiEnabled.boolValue = !menuWifiEnabled.boolValue;
        Serial.print("Menu: WiFi toggled ");
        Serial.println(menuWifiEnabled.boolValue ? "ON" : "OFF");

        if (menuWifiEnabled.boolValue) {
            bool apStarted = wifiStartAp();
            delay(200);
            Serial.print("  AP started: ");
            Serial.println(apStarted ? "yes" : "no");
            Serial.print("  AP IP: ");
            Serial.println(wifiGetApIP().c_str());
            Serial.print("  AP SSID: ");
            Serial.println(wifiGetApSSID().c_str());
            
            startCaptivePortal();
            wifiStartAutoConnectScan();
        } else {
            stopCaptivePortal();
            
            wifiDisconnect();
            wifiStopAp();
            wifiSetMode(WIFI_OFF);
            Serial.println("  WiFi disabled");
        }

        saveAllSettings();
        menuState.needsRedraw = true;
    }

    void cbWifiStatus() {
        updateWifiStatusText();
        menuState.needsRedraw = true;
    }

    void cbWifiScan() {
        static unsigned long lastScanAttempt = 0;
        static int scanRetryCount = 0;
        
        Serial.println("Menu: WiFi scan triggered");

        if (!menuWifiEnabled.boolValue) {
            Serial.println("  WiFi is OFF - enable first");
            beepLowC(100);
            return;
        }
        
        // Exponential backoff: min 1s, max 30s between manual scan attempts
        unsigned long cooldown = min(1000 * (1 << scanRetryCount), 30000);
        if (millis() - lastScanAttempt < cooldown) {
            Serial.printf("  Scan cooldown active, wait %lums\n", cooldown - (millis() - lastScanAttempt));
            return;
        }
        
        lastScanAttempt = millis();
        scanRetryCount++;

        // Start asynchronous scan
        if (!wifiIsApActive()) {
            wifiSetMode(WIFI_AP_STA);
        }
        delay(100);

        Serial.println("  Starting asynchronous WiFi scan...");
        wifiScan();  // Use WiFiHandler for async scan

        menuState.needsRedraw = true;
        
        // Reset retry count on successful scan start
        scanRetryCount = 0;
    }

    void cbWifiConnect() {
        Serial.println("Menu: WiFi Connect action!");

        if (!menuWifiEnabled.boolValue) {
            Serial.println("  WiFi is OFF - enable first");
            beepLowC(100);
            return;
        }

        const char* ssid = nullptr;
        const char* password = "";
        wifi_auth_mode_t encryption = WIFI_AUTH_OPEN;
        bool isSavedNetwork = false;

        // Get SSID from scanned or saved network
        if (isInScannedNetwork && selectedNetworkIndex >= 0 && selectedNetworkIndex < numScanResults) {
            ssid = scanResults[selectedNetworkIndex].ssid;
            encryption = scanResults[selectedNetworkIndex].encryption;
        } else if (isInSavedNetwork && selectedNetworkIndex >= 0 && selectedNetworkIndex < numSavedNetworks) {
            ssid = savedNetworks[selectedNetworkIndex].ssid;
            password = savedNetworks[selectedNetworkIndex].password;
            encryption = WIFI_AUTH_OPEN;
            isSavedNetwork = true;
        } else {
            Serial.println("[WiFi] Invalid network selection");
            return;
        }

        if (ssid == nullptr || strlen(ssid) == 0) {
            Serial.println("[WiFi] No SSID selected");
            return;
        }

        Serial.print("[WiFi] Connecting to: ");
        Serial.println(ssid);

        // For saved networks, verify SSID still exists in scan
        if (isSavedNetwork) {
            bool foundInScan = false;
            for (int i = 0; i < numScanResults; i++) {
                if (strcmp(scanResults[i].ssid, ssid) == 0) {
                    foundInScan = true;
                    encryption = scanResults[i].encryption;
                    break;
                }
            }
            if (!foundInScan) {
                Serial.println("[WiFi] Network not found in current scan - may be out of range");
                beepLowC(100);
                return;
            }
        }

        // Check if we need password input
        if (!isSavedNetwork && encryption != WIFI_AUTH_OPEN && (password == nullptr || strlen(password) == 0)) {
            // Need password for this network - direct to captive portal
            Serial.println("[WiFi] Password required - use captive portal");
            Serial.print("[WiFi] Network: ");
            Serial.println(ssid);
            Serial.println("[WiFi] Connect via web interface at http://192.168.4.1");
            beepLowC(100);
            return;
        }

        // Attempt connection - non-blocking, status via getConnectionStatus()
        if (encryption == WIFI_AUTH_OPEN || (password != nullptr && strlen(password) > 0)) {
            if (encryption == WIFI_AUTH_OPEN) {
                Serial.println("[WiFi] Connecting to open network (non-blocking)...");
                Serial.print("[WiFi] SSID: ");
                Serial.println(ssid);
                wifiConnectAsync(ssid, nullptr);
            } else {
                Serial.println("[WiFi] Connecting to secured network (non-blocking)...");
                Serial.print("[WiFi] SSID: ");
                Serial.println(ssid);
                wifiConnectAsync(ssid, password);
            }
            
            // Don't wait - connection status is now tracked in wifiTask
            // User can see status via getConnectionStatus() if needed
        } else {
            Serial.println("[WiFi] Network requires authentication but no password available.");
            beepLowC(100);
        }

        // Ensure AP is still running (non-blocking)
        wifiSetMode(WIFI_AP_STA);
        // Note: softAP will be started by wifiTask after mode change

        menuState.needsRedraw = true;
    }

    void cbWifiForget() {
        Serial.println("Menu: WiFi Forget action!");
        
        int networkIdx = selectedNetworkIndex;

        // Only allow forgetting from saved networks
        if (isInSavedNetwork && networkIdx >= 0 && networkIdx < numSavedNetworks) {
            Serial.print("  Forgetting network: ");
            Serial.println(savedNetworks[networkIdx].ssid);
            forgetWifiNetwork(networkIdx);
            updateSavedNetworksList();
            cleanupGhostNetworks();
            
            // Go back after forgetting
            menuGoBack();
            menuState.needsRedraw = true;
        }
    }

    void cbWifiDisconnect() {
        Serial.println("Menu: WiFi Disconnect");
        wifiDisconnect();
        snprintf(wifiStatusText, sizeof(wifiStatusText), "Status: DISCONNECTED");
        menuState.needsRedraw = true;
    }

    void cbAutoConnectToggle() {
        Serial.println("Menu: Auto-connect toggle!");
        
        int networkIdx = selectedNetworkIndex;
        
        // Only allow from saved networks
        if (isInSavedNetwork && networkIdx >= 0 && networkIdx < numSavedNetworks) {
            bool newValue = !savedNetworks[networkIdx].autoConnect;
            const char* targetSsid = savedNetworks[networkIdx].ssid;
            if (!wifiSetSavedNetworkAutoConnectBySsid(targetSsid, newValue)) {
                Serial.println("  Failed to update auto-connect");
                return;
            }
            wifiReloadSavedNetworks();
            updateSavedNetworksList();

            for (int i = 0; i < numSavedNetworks; i++) {
                if (strcmp(savedNetworks[i].ssid, targetSsid) == 0) {
                    selectedNetworkIndex = i;
                    menuState.selectedItem = &menuSavedAutoConnect[i];
                    Serial.print("  Auto-connect: ");
                    Serial.println(savedNetworks[i].autoConnect ? "ON" : "OFF");
                    menuState.needsRedraw = true;
                    return;
                }
            }
            
            menuState.needsRedraw = true;
            return;
        }
        
        // Check directly
        MenuItem* currentItem = menuState.selectedItem;
        for (int i = 0; i < numSavedNetworks; i++) {
            if (currentItem == &menuSavedAutoConnect[i]) {
                networkIdx = i;
                bool newValue = !savedNetworks[networkIdx].autoConnect;
                const char* targetSsid = savedNetworks[networkIdx].ssid;
                if (!wifiSetSavedNetworkAutoConnectBySsid(targetSsid, newValue)) {
                    Serial.println("  Failed to update auto-connect");
                    return;
                }
                wifiReloadSavedNetworks();
                updateSavedNetworksList();

                for (int j = 0; j < numSavedNetworks; j++) {
                    if (strcmp(savedNetworks[j].ssid, targetSsid) == 0) {
                        selectedNetworkIndex = j;
                        menuState.selectedItem = &menuSavedAutoConnect[j];
                        Serial.print("  Auto-connect: ");
                        Serial.println(savedNetworks[j].autoConnect ? "ON" : "OFF");
                        menuState.needsRedraw = true;
                        return;
                    }
                }

                menuState.needsRedraw = true;
                return;
            }
        }
    }

    void cbShowQR() {
        Serial.println("Menu: Show QR code");
        menuState.showQRCode = true;
        menuState.needsRedraw = false;
    }

    void cbShowSSID() {
        Serial.println("Menu: Show SSID");
    }

    void cbShowPassword() {
        Serial.println("Menu: Show Password");
    }

    void cbBluetoothToggle() {
        menuBtEnabled.boolValue = !menuBtEnabled.boolValue;
        Serial.print("Menu: Bluetooth toggled ");
        Serial.println(menuBtEnabled.boolValue ? "ON" : "OFF");
        saveAllSettings();
        menuState.needsRedraw = true;
    }

    void cbMSAuth() {
        Serial.println("Menu: MSAuth action selected");

        // Check if WiFi is connected - required for NTP sync
        // Use WiFiHandler status
        if (!wifiIsConnected()) {
            Serial.println("[TOTP] WiFi not connected - cannot generate valid TOTP codes");
            Serial.println("[TOTP] Please connect to WiFi first via Wifi menu");
            beepLowC(100);
            return;
        }

        // Show TOTP code display
        if (!hasTotpKey()) {
            Serial.println("[TOTP] No TOTP secret available - set TOTP_HEX_SECRET first");
            beepLowC(100);
            return;
        }

        // Force NTP sync before showing code
        Serial.println("[TOTP] Syncing NTP time before generating code...");
        // syncNtpTime() handled by WiFiHandler

        showTotpCode = true;
        menuState.needsRedraw = true;
        generateTotpCode();  // Generate initial code
        Serial.println("[TOTP] Showing TOTP code display");
    }
    
    void cbTotpSetup() {
        Serial.println("Menu: TOTP Setup Secret selected");
        showTotpSecret = true;
        menuState.needsRedraw = true;
        Serial.println("[TOTP] Showing TOTP secret for MS Authenticator setup");
    }

    void cbSaveSettings() {
        saveAllSettings();
        Serial.println("Menu: Settings saved!");
        menuState.needsRedraw = true;
    }

    void cbBrightness() {
        Serial.print("Menu: Brightness set to ");
        Serial.println(menuBrightness.sliderValue);
        menuState.needsRedraw = true;
    }

    void cbBluetoothTest() {
        Serial.println("Menu: Bluetooth Test action!");
        if (menuBtEnabled.boolValue) {
            Serial.println("  Bluetooth is ON - test functionality here");
        } else {
            Serial.println("  Bluetooth is OFF - enable first");
            beepLowC(100);
        }
        menuState.needsRedraw = true;
    }

    void cbWifiTest() {
        Serial.println("Menu: WiFi Test action!");
        if (menuWifiEnabled.boolValue) {
            Serial.print("  WiFi is ON, status: ");
            Serial.println(wifiIsConnected() ? "CONNECTED" : "NOT CONNECTED");
        } else {
            Serial.println("  WiFi is OFF - enable first");
            beepLowC(100);
        }
        menuState.needsRedraw = true;
    }

    // Update TOTP timer to run in the background
    void updateTotpInBackground() {
        unsigned long currentMillis = millis();

        // NTP sync handled by WiFiHandler

        // Only update code if time is synced
        time_t now = time(nullptr);
        if (now < 1000000000) {
            return; // Time not synced yet
        }

        // Check if it's time to update the TOTP code
        if (currentMillis - lastTotpUpdate >= TOTP_UPDATE_INTERVAL) {
            lastTotpUpdate = currentMillis;
            generateTotpCode();
        }
    }
