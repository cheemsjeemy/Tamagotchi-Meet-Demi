#include "WiFiHandler.h"
#include <Preferences.h>
#include <time.h>
#include "captive_portal.h"

bool captivePortalRoutesRegistered = false;
bool captivePortalRunning = false;
DNSServer dnsServer;

// Non-blocking WiFi connection variables
bool shouldConnect = false;
String targetSSID = "";
String targetPass = "";

static const long GMT_OFFSET_SEC = 28800;
static const int DAYLIGHT_OFFSET_SEC = 0;
static const char* PREF_NAMESPACE = "demi settings";

extern void startCaptivePortal();

QueueHandle_t wifiCommandQueue = nullptr;

WifiStatus wifiStatus = {};

WifiScanResult scanResults[MAX_WIFI_SCAN_RESULTS] = {};
int numScanResults = 0;
bool isWifiScanning = false;
bool isAutoConnectScan = false;
bool noSavedNetworksInRange = false;

WifiScanResult savedNetworks[MAX_SAVED_NETWORKS] = {};
int numSavedNetworks = 0;

int selectedNetworkIndex = -1;
bool isInScannedNetwork = false;
bool isInSavedNetwork = false;
bool scaryNetworkFound = false;

static bool ntpSyncInProgress = false;
static unsigned long lastNtpAttempt = 0;

static void updateStatusString(const char* status) {
    strncpy(wifiStatus.statusString, status, sizeof(wifiStatus.statusString) - 1);
    wifiStatus.statusString[sizeof(wifiStatus.statusString) - 1] = '\0';
}

static void clearSavedNetworkSlot(Preferences& prefs, int index) {
    char key[24];

    snprintf(key, sizeof(key), "wifi_ssid_%d", index);
    prefs.remove(key);

    snprintf(key, sizeof(key), "wifi_pass_%d", index);
    prefs.remove(key);

    snprintf(key, sizeof(key), "wifi_auto_%d", index);
    prefs.remove(key);
}

static void writeSavedNetworkSlot(Preferences& prefs, int index, const WifiScanResult& network) {
    char key[24];

    snprintf(key, sizeof(key), "wifi_ssid_%d", index);
    prefs.putString(key, network.ssid);

    snprintf(key, sizeof(key), "wifi_pass_%d", index);
    prefs.putString(key, network.password);

    snprintf(key, sizeof(key), "wifi_auto_%d", index);
    prefs.putBool(key, network.autoConnect);
}

static void persistSavedNetworks() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, false);
    prefs.putInt("wifi_net_count", numSavedNetworks);

    for (int i = 0; i < numSavedNetworks; ++i) {
        writeSavedNetworkSlot(prefs, i, savedNetworks[i]);
    }

    for (int i = numSavedNetworks; i < MAX_SAVED_NETWORKS; ++i) {
        clearSavedNetworkSlot(prefs, i);
    }

    prefs.end();
}

static void loadSavedNetworksFromPrefs() {
    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);

    int savedCount = prefs.getInt("wifi_net_count", 0);
    numSavedNetworks = 0;
    memset(savedNetworks, 0, sizeof(savedNetworks));

    for (int i = 0; i < savedCount && i < MAX_SAVED_NETWORKS; ++i) {
        char key[24];

        snprintf(key, sizeof(key), "wifi_ssid_%d", i);
        String ssid = prefs.getString(key, "");
        if (ssid.isEmpty()) {
            continue;
        }

        WifiScanResult& network = savedNetworks[numSavedNetworks];
        strncpy(network.ssid, ssid.c_str(), sizeof(network.ssid) - 1);

        snprintf(key, sizeof(key), "wifi_pass_%d", i);
        String password = prefs.getString(key, "");
        strncpy(network.password, password.c_str(), sizeof(network.password) - 1);

        snprintf(key, sizeof(key), "wifi_auto_%d", i);
        network.autoConnect = prefs.getBool(key, false);
        network.encryption = WIFI_AUTH_OPEN;
        network.isConnected = false;
        network.rssi = 0;

        Serial.printf("[WiFiHandler] Loaded saved network %d: %s (Auto: %s)\n",
                      numSavedNetworks,
                      network.ssid,
                      network.autoConnect ? "ON" : "OFF");

        ++numSavedNetworks;
    }

    prefs.end();
}

void wifiReloadSavedNetworks() {
    loadSavedNetworksFromPrefs();
}

static void actualSetMode(wifi_mode_t mode) {
    WiFi.mode(mode);
    Serial.printf("[WiFiHandler] Mode set to: %d\n", static_cast<int>(mode));
}

static void actualDisconnect() {
    WiFi.disconnect(true, true);
    wifiStatus.state = WIFI_STATE_DISCONNECTED;
    wifiStatus.connectionPending = false;
    wifiStatus.ntpSynced = false;
    wifiStatus.lastSyncTime = 0;
    wifiStatus.currentSsid[0] = '\0';
    wifiStatus.pendingSsid[0] = '\0';
    wifiStatus.pendingPassword[0] = '\0';
    wifiStatus.localIp = IPAddress();
    updateStatusString("Disconnected");
    Serial.println("[WiFiHandler] Disconnected");
}

static void actualStartScan(bool autoConnectScan) {
    if (isWifiScanning) {
        Serial.println("[WiFiHandler] Scan already in progress");
        return;
    }

    if ((WiFi.getMode() & WIFI_STA) == 0) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }

    isWifiScanning = true;
    isAutoConnectScan = autoConnectScan;
    noSavedNetworksInRange = false;
    wifiStatus.state = WIFI_STATE_SCANNING;
    updateStatusString(autoConnectScan ? "Auto-scanning..." : "Scanning...");

    WiFi.scanDelete();
    WiFi.scanNetworks(true);
    Serial.println(autoConnectScan ? "[WiFiHandler] Auto-connect scan started"
                                   : "[WiFiHandler] Async scan started");
}

static void handleConnectionStatus() {
    if (!wifiStatus.connectionPending) {
        return;
    }

    wl_status_t status = WiFi.status();
    unsigned long elapsed = millis() - wifiStatus.connectionStartTime;

    if (status == WL_CONNECTED) {
        wifiStatus.connectionPending = false;
        wifiStatus.state = WIFI_STATE_CONNECTED;
        wifiStatus.ntpSynced = false;
        wifiStatus.lastSyncTime = 0;
        wifiStatus.lastAttempt = millis();
        strncpy(wifiStatus.currentSsid, WiFi.SSID().c_str(), sizeof(wifiStatus.currentSsid) - 1);
        wifiStatus.currentSsid[sizeof(wifiStatus.currentSsid) - 1] = '\0';
        wifiStatus.localIp = WiFi.localIP();

        snprintf(wifiStatus.statusString, sizeof(wifiStatus.statusString), "Connected");
        Serial.printf("[WiFiHandler] Connected to: %s\n", wifiStatus.currentSsid);

        // Trigger immediate NTP sync on connect
        configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "time.google.com", "time.cloudflare.com", "pool.ntp.org");
        ntpSyncInProgress = true;
        lastNtpAttempt = millis();
        Serial.println("[WiFiHandler] NTP sync triggered on connect");


        return;

    }

    if (elapsed < WIFI_CONNECT_TIMEOUT_MS &&
        (status == WL_IDLE_STATUS || status == WL_DISCONNECTED)) {
        return;
    }

    wifiStatus.connectionPending = false;
    wifiStatus.state = WIFI_STATE_ERROR;
    updateStatusString(status == WL_NO_SSID_AVAIL ? "SSID Missing"
                     : status == WL_CONNECT_FAILED ? "Failed"
                     : "Timeout");
    Serial.printf("[WiFiHandler] Connection failed with status %d\n", static_cast<int>(status));
}

static void handleNtpSync() {
    if (wifiStatus.state != WIFI_STATE_CONNECTED || wifiStatus.ntpSynced) {
        return;
    }

    if (ntpSyncInProgress) {
        time_t now = time(nullptr);
        if (now >= 1600000000) {
            wifiStatus.lastSyncTime = now;
            wifiStatus.ntpSynced = true;
            ntpSyncInProgress = false;
            Serial.println("[WiFiHandler] NTP synced");
            
            extern void syncRTCFromUnixTime(time_t);
            syncRTCFromUnixTime(now);
        } else if (millis() - lastNtpAttempt > NTP_SYNC_TIMEOUT_MS) {
            ntpSyncInProgress = false;
            Serial.println("[WiFiHandler] NTP sync timeout");
        }
        return;
    }

    if (millis() - lastNtpAttempt < NTP_RETRY_INTERVAL_MS) {
        return;
    }

    lastNtpAttempt = millis();
    ntpSyncInProgress = true;
    configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, "time.google.com", "time.cloudflare.com", "pool.ntp.org");
    Serial.println("[WiFiHandler] NTP sync started");
}

bool isWifiNetworkSaved(const char* ssid) {
    if (ssid == nullptr || ssid[0] == '\0') {
        return false;
    }

    for (int i = 0; i < numSavedNetworks; ++i) {
        if (strcmp(savedNetworks[i].ssid, ssid) == 0) {
            return true;
        }
    }

    return false;
}

bool wifiSetSavedNetworkAutoConnect(int index, bool enabled) {
    if (index < 0 || index >= numSavedNetworks) {
        Serial.printf("[WiFiHandler] Invalid auto-connect index: %d\n", index);
        return false;
    }

    savedNetworks[index].autoConnect = enabled;
    persistSavedNetworks();
    Serial.printf("[WiFiHandler] Auto-connect for %s set to %s\n",
                  savedNetworks[index].ssid,
                  enabled ? "ON" : "OFF");
    return true;
}

bool wifiSetSavedNetworkAutoConnectBySsid(const char* ssid, bool enabled) {
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("[WiFiHandler] Cannot set auto-connect for empty SSID");
        return false;
    }

    loadSavedNetworksFromPrefs();

    for (int i = 0; i < numSavedNetworks; ++i) {
        if (strcmp(savedNetworks[i].ssid, ssid) == 0) {
            return wifiSetSavedNetworkAutoConnect(i, enabled);
        }
    }

    Serial.printf("[WiFiHandler] Saved SSID not found for auto-connect: %s\n", ssid);
    return false;
}

void cleanupGhostNetworks() {
    int writeIndex = 0;

    for (int i = 0; i < numSavedNetworks; ++i) {
        if (savedNetworks[i].ssid[0] == '\0') {
            continue;
        }

        if (writeIndex != i) {
            savedNetworks[writeIndex] = savedNetworks[i];
            memset(&savedNetworks[i], 0, sizeof(savedNetworks[i]));
        }
        ++writeIndex;
    }

    numSavedNetworks = writeIndex;
    persistSavedNetworks();
}

void saveWifiNetwork(const char* ssid, const char* password) {
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("[WiFiHandler] Refusing to save empty SSID");
        return;
    }

    for (int i = 0; i < numSavedNetworks; ++i) {
        if (strcmp(savedNetworks[i].ssid, ssid) == 0) {
            strncpy(savedNetworks[i].password, password != nullptr ? password : "", sizeof(savedNetworks[i].password) - 1);
            savedNetworks[i].password[sizeof(savedNetworks[i].password) - 1] = '\0';
            persistSavedNetworks();
            Serial.printf("[WiFiHandler] Updated saved network: %s\n", ssid);
            return;
        }
    }

    if (numSavedNetworks >= MAX_SAVED_NETWORKS) {
        Serial.println("[WiFiHandler] Saved network list full, replacing the oldest slot");
        numSavedNetworks = MAX_SAVED_NETWORKS - 1;
    }

    WifiScanResult& network = savedNetworks[numSavedNetworks];
    memset(&network, 0, sizeof(network));
    strncpy(network.ssid, ssid, sizeof(network.ssid) - 1);
    strncpy(network.password, password != nullptr ? password : "", sizeof(network.password) - 1);
    network.encryption = WIFI_AUTH_OPEN;
    network.autoConnect = false;
    ++numSavedNetworks;

    cleanupGhostNetworks();
    Serial.printf("[WiFiHandler] Saved network: %s\n", ssid);
}

void forgetWifiNetwork(int index) {
    if (index < 0 || index >= numSavedNetworks) {
        Serial.printf("[WiFiHandler] Invalid saved network index: %d\n", index);
        return;
    }

    for (int i = index; i < numSavedNetworks - 1; ++i) {
        savedNetworks[i] = savedNetworks[i + 1];
    }

    if (numSavedNetworks > 0) {
        memset(&savedNetworks[numSavedNetworks - 1], 0, sizeof(savedNetworks[numSavedNetworks - 1]));
        --numSavedNetworks;
    }

    if (selectedNetworkIndex >= numSavedNetworks) {
        selectedNetworkIndex = numSavedNetworks - 1;
    }

    persistSavedNetworks();
    Serial.println("[WiFiHandler] Saved network removed");
}

void performAutoConnect() {
    loadSavedNetworksFromPrefs();

    int bestSavedIndex = -1;
    int bestRssi = -1000;

    Serial.printf("[WiFiHandler] Evaluating %d saved networks against %d scanned networks\n",
                  numSavedNetworks,
                  numScanResults);

    for (int i = 0; i < numScanResults; ++i) {
        for (int j = 0; j < numSavedNetworks; ++j) {
            if (!savedNetworks[j].autoConnect) {
                continue;
            }

            Serial.printf("[WiFiHandler] Checking saved '%s' against scanned '%s' (RSSI: %d)\n",
                          savedNetworks[j].ssid,
                          scanResults[i].ssid,
                          scanResults[i].rssi);

            if (strcmp(scanResults[i].ssid, savedNetworks[j].ssid) == 0 &&
                scanResults[i].rssi > bestRssi) {
                bestRssi = scanResults[i].rssi;
                bestSavedIndex = j;
                Serial.printf("[WiFiHandler] Candidate auto-connect match: %s (RSSI: %d)\n",
                              savedNetworks[j].ssid,
                              bestRssi);
            }
        }
    }

    noSavedNetworksInRange = (bestSavedIndex < 0);
    if (bestSavedIndex < 0) {
        Serial.println("[WiFiHandler] No auto-connect networks in range");
        updateStatusString("No Networks");
        return;
    }

    Serial.printf("[WiFiHandler] Auto-connecting to: %s (RSSI: %d)\n",
                  savedNetworks[bestSavedIndex].ssid,
                  bestRssi);
    wifiConnect(savedNetworks[bestSavedIndex].ssid, savedNetworks[bestSavedIndex].password);
}

static void handleScanComplete() {
    int scanCount = WiFi.scanComplete();
    if (scanCount == WIFI_SCAN_RUNNING || scanCount == WIFI_SCAN_FAILED) {
        if (scanCount == WIFI_SCAN_FAILED) {
            isWifiScanning = false;
            isAutoConnectScan = false;
            wifiStatus.state = WIFI_STATE_ERROR;
            updateStatusString("Scan Failed");
            Serial.println("[WiFiHandler] Scan failed");
        }
        return;
    }

    numScanResults = (scanCount > MAX_WIFI_SCAN_RESULTS) ? MAX_WIFI_SCAN_RESULTS : scanCount;

    for (int i = 0; i < numScanResults; ++i) {
        WifiScanResult& result = scanResults[i];
        memset(&result, 0, sizeof(result));
        strncpy(result.ssid, WiFi.SSID(i).c_str(), sizeof(result.ssid) - 1);
        result.rssi = WiFi.RSSI(i);
        result.encryption = WiFi.encryptionType(i);
        result.isConnected = (WiFi.status() == WL_CONNECTED && WiFi.SSID() == result.ssid);

        for (int j = 0; j < numSavedNetworks; ++j) {
            if (strcmp(savedNetworks[j].ssid, result.ssid) == 0) {
                result.autoConnect = savedNetworks[j].autoConnect;
                break;
            }
        }
    }

    for (int i = numScanResults; i < MAX_WIFI_SCAN_RESULTS; ++i) {
        memset(&scanResults[i], 0, sizeof(scanResults[i]));
    }

    // Check for scary networks (hidden SSIDs, strange names)
    scaryNetworkFound = false;
    for (int i = 0; i < numScanResults; ++i) {
        // Hidden networks = spooky
        if (strlen(scanResults[i].ssid) == 0) {
            scaryNetworkFound = true;
            Serial.println("[WiFiHandler] ⚠️  Hidden network detected! Demi is spooked...");
            break;
        }
        
        // Generic "unknown" or "scary" network names
        const char* scaryNames[] = {"unknown", "hidden", "virus", "hack", "ghost", "evil", "demon", "spooky", NULL};
        for (int j = 0; scaryNames[j] != NULL; j++) {
            if (strcasestr(scanResults[i].ssid, scaryNames[j]) != NULL) {
                scaryNetworkFound = true;
                Serial.printf("[WiFiHandler] ⚠️  Spooky network detected: '%s'! Demi is scared!\n", scanResults[i].ssid);
                break;
            }
        }
        if (scaryNetworkFound) break;
    }

    WiFi.scanDelete();
    isWifiScanning = false;
    wifiStatus.state = wifiIsConnected() ? WIFI_STATE_CONNECTED : WIFI_STATE_IDLE;
    updateStatusString(wifiIsConnected() ? "Connected" : "Idle");
    Serial.printf("[WiFiHandler] Scan complete, found %d networks\n", numScanResults);

    bool runAutoConnect = isAutoConnectScan;
    isAutoConnectScan = false;
    if (runAutoConnect) {
        performAutoConnect();
    }
}

static void processCommand(const WifiCommand& cmd) {
    switch (cmd.type) {
        case WIFI_CMD_CONNECT:
            if (cmd.password[0] != '\0') {
                WiFi.begin(cmd.ssid, cmd.password);
            } else {
                WiFi.begin(cmd.ssid);
            }

            wifiStatus.connectionPending = true;
            wifiStatus.connectionStartTime = millis();
            wifiStatus.state = WIFI_STATE_CONNECTING;
            strncpy(wifiStatus.pendingSsid, cmd.ssid, sizeof(wifiStatus.pendingSsid) - 1);
            strncpy(wifiStatus.pendingPassword, cmd.password, sizeof(wifiStatus.pendingPassword) - 1);
            updateStatusString("Connecting...");
            Serial.printf("[WiFiHandler] Connecting to: %s\n", cmd.ssid);
            
            break;

        case WIFI_CMD_DISCONNECT:
            actualDisconnect();
            break;

        case WIFI_CMD_SCAN:
            actualStartScan(false);
            break;

        case WIFI_CMD_SET_MODE:
            actualSetMode(cmd.mode);
            break;

        case WIFI_CMD_AUTO_CONNECT:
            actualStartScan(true);
            break;

        case WIFI_CMD_CHECK_STATUS:
        case WIFI_CMD_NONE:
        default:
            break;
    }
}

static bool enqueueCommand(const WifiCommand& cmd) {
    if (wifiCommandQueue == nullptr) {
        processCommand(cmd);
        return true;
    }

    if (xQueueSend(wifiCommandQueue, &cmd, 0) == pdTRUE) {
        return true;
    }

    Serial.println("[WiFiHandler] Failed to queue WiFi command");
    return false;
}

void initWifiHandler() {
    Serial.println("[WiFiHandler] Initializing...");

    memset(&wifiStatus, 0, sizeof(wifiStatus));
    wifiStatus.state = WIFI_STATE_IDLE;
    updateStatusString("Idle");

    loadSavedNetworksFromPrefs();

    Preferences prefs;
    prefs.begin(PREF_NAMESPACE, true);
    bool wifiEnabled = prefs.getBool("wifi_enabled", false);
    bool wifiModeAP = prefs.getBool("wifi_mode_ap", false); // false = STA, true = AP only
    prefs.end();

    Serial.printf("[WiFiHandler] Mode: %s\n", wifiModeAP ? "AP Only" : "STA + AP");

    if (wifiEnabled) {
        // If wifiModeAP is true, use pure AP mode. Otherwise use AP+STA for internet access
        if (wifiModeAP) {
            WiFi.mode(WIFI_AP); // Pure AP for captive portal
        } else {
            WiFi.mode(WIFI_AP_STA); // Both AP and STA
        }
        delay(100);

        // Explicitly configure SoftAP IP to ensure DHCP/DNS consistency
        IPAddress local_IP = IPAddress(192, 168, 4, 1);
        IPAddress gateway = IPAddress(192, 168, 4, 1);
        IPAddress subnet = IPAddress(255, 255, 255, 0);
        WiFi.softAPConfig(local_IP, gateway, subnet);

        WiFi.softAP("Demi-ESP32", nullptr);  // Open network - no password
        wifiStatus.state = WIFI_STATE_AP_MODE;
        updateStatusString("AP Mode");
        startCaptivePortal();
        
        // Only start auto-connect scan if in STA mode (not AP-only mode)
        if (!wifiModeAP) {
            actualStartScan(true); // Auto-connect scan
        }
    }

    Serial.println("[WiFiHandler] Initialization complete");
}

void deinitWifiHandler() {
    actualDisconnect();
    WiFi.mode(WIFI_OFF);
    wifiStatus.state = WIFI_STATE_IDLE;
    updateStatusString("Disabled");
}

void wifiConnect(const char* ssid, const char* password) {
    if (ssid == nullptr || ssid[0] == '\0') {
        Serial.println("[WiFiHandler] Refusing to connect without an SSID");
        return;
    }

    WifiCommand cmd = {};
    cmd.type = WIFI_CMD_CONNECT;
    strncpy(cmd.ssid, ssid, sizeof(cmd.ssid) - 1);
    if (password != nullptr) {
        strncpy(cmd.password, password, sizeof(cmd.password) - 1);
    }
    cmd.hasPassword = (cmd.password[0] != '\0');
    cmd.startTime = millis();
    enqueueCommand(cmd);
}

void wifiConnectAsync(const char* ssid, const char* password) {
    wifiConnect(ssid, password);
}

void wifiConnectNoPassword(const char* ssid) {
    wifiConnect(ssid, "");
}

void wifiDisconnect() {
    WifiCommand cmd = {};
    cmd.type = WIFI_CMD_DISCONNECT;
    enqueueCommand(cmd);
}

void wifiStartScan() {
    WifiCommand cmd = {};
    cmd.type = WIFI_CMD_SCAN;
    enqueueCommand(cmd);
}

void wifiScan() {
    wifiStartScan();
}

void wifiStartAutoConnectScan() {
    WifiCommand cmd = {};
    cmd.type = WIFI_CMD_AUTO_CONNECT;
    enqueueCommand(cmd);
}

void wifiSetMode(wifi_mode_t mode) {
    WifiCommand cmd = {};
    cmd.type = WIFI_CMD_SET_MODE;
    cmd.mode = mode;
    enqueueCommand(cmd);
}

const char* wifiGetStatusString() {
    return wifiStatus.statusString;
}

WifiState wifiGetState() {
    return wifiStatus.state;
}

bool wifiIsConnected() {
    return WiFi.status() == WL_CONNECTED;
}

bool wifiIsNtpSynced() {
    return wifiStatus.ntpSynced;
}

time_t wifiGetTime() {
    if (wifiStatus.ntpSynced && wifiStatus.lastSyncTime > 0) {
        unsigned long elapsed = (millis() - wifiStatus.lastSyncTime) / 1000;
        return wifiStatus.lastSyncTime + elapsed;
    }
    return 0;
}

bool wifiShouldSyncNtp() {
    return wifiStatus.state == WIFI_STATE_CONNECTED && !wifiStatus.ntpSynced;
}

String wifiGetApSSID() {
    return WiFi.softAPSSID();
}

String wifiGetApIP() {
    return WiFi.softAPIP().toString();
}

bool wifiIsApActive() {
    wifi_mode_t mode = WiFi.getMode();
    return (mode & WIFI_AP) != 0;
}

bool wifiStartAp() {
    if (!wifiIsApActive()) {
        WiFi.mode(WIFI_AP_STA);
        delay(100);
    }
    return WiFi.softAP("Demi-ESP32", nullptr);  // Open network - no password
}

void wifiStopAp() {
    if (wifiIsApActive()) {
        WiFi.softAPdisconnect(true);
    }
}

String wifiGetCurrentSSID() {
    return WiFi.SSID();
}

IPAddress wifiGetLocalIP() {
    return WiFi.localIP();
}

void processCaptivePortalDNS() {
    if (captivePortalRunning) {
        dnsServer.processNextRequest();
        // Debug: periodically log DNS activity
        static unsigned long lastDnsLog = 0;
        if (millis() - lastDnsLog > 10000) {
            // Just a heartbeat to show DNS is running
            Serial.printf("[DNS] Server running. AP IP: %s, Stations: %d\n", 
                WiFi.softAPIP().toString().c_str(), WiFi.softAPgetStationNum());
            lastDnsLog = millis();
        }
    }
}

void wifiHandlerTask(void* param) {
    (void)param;
    Serial.println("[WiFiHandler] Task started on Core 0");

    initWifiHandler();

    while (true) {
        WifiCommand cmd = {};
        if (wifiCommandQueue != nullptr &&
            xQueueReceive(wifiCommandQueue, &cmd, pdMS_TO_TICKS(50)) == pdTRUE) {
            processCommand(cmd);
        }

        if (isWifiScanning) {
            handleScanComplete();
        }

        handleConnectionStatus();
        handleNtpSync();

        // Process Captive Portal DNS requests on the same core as WiFi
        extern void processCaptivePortalDNS();
        processCaptivePortalDNS();

        // Periodically log connected stations for diagnostic purposes
        static unsigned long lastStationLog = 0;
        if (millis() - lastStationLog > 5000) {
            int count = WiFi.softAPgetStationNum();
            if (count > 0) {
                Serial.printf("[WiFiHandler] Connected stations: %d\n", count);
            }
            lastStationLog = millis();
        }

         vTaskDelay(pdMS_TO_TICKS(20));
    }
}

// Process pending WiFi connection from loop() - non-blocking approach
// This is called repeatedly from the main loop to avoid blocking the web server
void processPendingWiFiConnection() {
    if (shouldConnect) {
        Serial.printf("[WiFi] Connecting to %s...\n", targetSSID.c_str());
        
        WiFi.disconnect();
        delay(100);
        
        WiFi.begin(targetSSID.c_str(), targetPass.c_str());
        
        shouldConnect = false;
    }
}

