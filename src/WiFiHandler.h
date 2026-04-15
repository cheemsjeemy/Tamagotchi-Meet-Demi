#ifndef WIFIHANDLER_H
#define WIFIHANDLER_H

#include <Arduino.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <esp_task.h>

#define WIFI_TASK_STACK_SIZE 8192
#define WIFI_TASK_PRIORITY 1
#define WIFI_TASK_CORE 0

#define WIFI_CONNECT_TIMEOUT_MS 10000
#define NTP_SYNC_TIMEOUT_MS 3000
#define NTP_RETRY_INTERVAL_MS 1000

#define MAX_WIFI_SCAN_RESULTS 15
#define MAX_SAVED_NETWORKS 5

extern QueueHandle_t wifiCommandQueue;

typedef enum {
    WIFI_STATE_IDLE,
    WIFI_STATE_CONNECTING,
    WIFI_STATE_CONNECTED,
    WIFI_STATE_DISCONNECTED,
    WIFI_STATE_SCANNING,
    WIFI_STATE_AP_MODE,
    WIFI_STATE_ERROR
} WifiState;

typedef enum {
    WIFI_CMD_NONE,
    WIFI_CMD_CONNECT,
    WIFI_CMD_DISCONNECT,
    WIFI_CMD_SCAN,
    WIFI_CMD_SET_MODE,
    WIFI_CMD_AUTO_CONNECT,
    WIFI_CMD_CHECK_STATUS
} WifiCommandType;

typedef struct {
    WifiCommandType type;
    char ssid[32];
    char password[64];
    wifi_mode_t mode;
    unsigned long startTime;
    bool hasPassword;
} WifiCommand;

typedef struct {
    char ssid[32];
    char password[64];
    int rssi;
    bool isConnected;
    bool autoConnect;
    wifi_auth_mode_t encryption;
} WifiScanResult;

typedef struct {
    WifiState state;
    char currentSsid[33];
    IPAddress localIp;
    char statusString[32];
    bool ntpSynced;
    time_t lastSyncTime;
    unsigned long lastAttempt;
    unsigned long connectionStartTime;
    bool connectionPending;
    char pendingSsid[33];
    char pendingPassword[64];
} WifiStatus;

extern WifiStatus wifiStatus;
extern WifiScanResult scanResults[MAX_WIFI_SCAN_RESULTS];
extern int numScanResults;
extern bool isWifiScanning;
extern bool isAutoConnectScan;
extern bool noSavedNetworksInRange;

extern WifiScanResult savedNetworks[MAX_SAVED_NETWORKS];
extern int numSavedNetworks;
extern int selectedNetworkIndex;
extern bool isInScannedNetwork;
extern bool isInSavedNetwork;
extern bool scaryNetworkFound;

void initWifiHandler();
void deinitWifiHandler();

void wifiConnect(const char* ssid, const char* password);
void wifiConnectAsync(const char* ssid, const char* password);
void wifiConnectNoPassword(const char* ssid);
void wifiDisconnect();
void wifiScan();
void wifiStartScan();
void wifiStartAutoConnectScan();
void wifiSetMode(wifi_mode_t mode);

const char* wifiGetStatusString();
WifiState wifiGetState();
bool wifiIsConnected();
bool wifiIsNtpSynced();
time_t wifiGetTime();

bool wifiShouldSyncNtp();

// AP Info helpers
String wifiGetApSSID();
String wifiGetApIP();
bool wifiIsApActive();
bool wifiStartAp();
void wifiStopAp();

// Network info helpers
String wifiGetCurrentSSID();
IPAddress wifiGetLocalIP();

void wifiHandlerTask(void* param);
void performAutoConnect();
void saveWifiNetwork(const char* ssid, const char* password);
void forgetWifiNetwork(int index);
void cleanupGhostNetworks();
bool isWifiNetworkSaved(const char* ssid);
bool wifiSetSavedNetworkAutoConnect(int index, bool enabled);
bool wifiSetSavedNetworkAutoConnectBySsid(const char* ssid, bool enabled);
void wifiReloadSavedNetworks();

#endif
