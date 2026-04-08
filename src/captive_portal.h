#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <DNSServer.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <esp_task.h>
#include "menu.h"
#include "html/html.h"

// External references (now defined in WiFiHandler.h via menu.h)
// isWifiScanning, scanResults, numScanResults are accessed via menu.h includes

// External references
extern Preferences preferences;
extern MenuState menuState;

// External function from menu.cpp
extern void saveWifiNetwork(const char* ssid, const char* password);

// Captive Portal Web Server on port 80
extern AsyncWebServer captiveServer;

// Captive Portal SSID and Password
#define CAPTIVE_AP_SSID "Demi-ESP32"
#define CAPTIVE_AP_PASSWORD "demiesp32"

// Custom domain for captive portal
#define CAPTIVE_DOMAIN "demi.connect"

static bool captivePortalRoutesRegistered = false;
static bool captivePortalRunning = false;

// DNS Server instance
static DNSServer dnsServer;

// Start captive portal web server
void startCaptivePortal() {
    if (captivePortalRunning) {
        Serial.println("[CaptivePortal] Web server already running");
        return;
    }

    Serial.println("[CaptivePortal] Starting web server...");

    if (!captivePortalRoutesRegistered) {
        // Root page - WiFi configuration form
        captiveServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
        
        request->send(200, "text/html", INDEX_HTML);
        });
        
        // Return available WiFi networks (use cached scan results from menu)
        captiveServer.on("/networks", HTTP_GET, [](AsyncWebServerRequest *request) {
        String json = "[";
        
        // Use cached scan results from menu system
        for (int i = 0; i < numScanResults; i++) {
            if (i > 0) json += ",";
            json += "{\"ssid\":\"";
            json += scanResults[i].ssid;
            json += "\",\"rssi\":";
            // Convert RSSI to percentage (approximate)
            int rssi = scanResults[i].rssi;
            int percentage = map(rssi, -100, -30, 0, 100);
            json += String(percentage) + "}";
        }
        json += "]";
        
        Serial.print("[CaptivePortal] /networks returning ");
        Serial.print(numScanResults);
        Serial.println(" networks");
        
        request->send(200, "application/json", json);
        });
        
        // Handle WiFi connection
        captiveServer.on("/connect", HTTP_POST, [](AsyncWebServerRequest *request) {
        if (request->hasParam("ssid", true) && request->hasParam("password", true)) {
            String ssid = request->getParam("ssid", true)->value();
            String password = request->getParam("password", true)->value();
            
            Serial.print("[CaptivePortal] Connecting to: ");
            Serial.println(ssid);
            
            // Store credentials
            preferences.putString("wifi_ssid", ssid.c_str());
            preferences.putString("wifi_password", password.c_str());
            
            // Disconnect first
            WiFi.disconnect();
            delay(100);
            
            // Connect to the new network
            WiFi.begin(ssid.c_str(), password.c_str());
            
            // Wait for connection (max 10 seconds)
            int attempts = 0;
            while (WiFi.status() != WL_CONNECTED && attempts < 20) {
                delay(500);
                Serial.print(".");
                attempts++;
            }
            Serial.println();
            
            if (WiFi.status() == WL_CONNECTED) {
                Serial.println("[CaptivePortal] Connected!");
                Serial.print("  IP: ");
                Serial.println(WiFi.localIP());

                // Save the network to menu with password
                saveWifiNetwork(ssid.c_str(), password.c_str());

                String response = "{\"success\":true,\"ip\":\"" + WiFi.localIP().toString() + "\"}";
                request->send(200, "application/json", response);
            } else {
                Serial.println("[CaptivePortal] Connection failed!");
                request->send(200, "application/json", "{\"success\":false,\"message\":\"Failed to connect. Check password.\"}");
            }
        } else {
            request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing parameters\"}");
        }
        });
        
        // Trigger WiFi scan (runs on Core 0)
        captiveServer.on("/scan", HTTP_GET, [](AsyncWebServerRequest *request) {
        Serial.println("[CaptivePortal] /scan - triggering WiFi scan on Core 0");
        
        // Set scanning flag
        isWifiScanning = true;
        
        // Use a static pointer to pass request to the task
        static AsyncWebServerRequest* pendingRequest = request;
        
        // Run WiFi scan on Core 0
        xTaskCreatePinnedToCore(
            [](void* param) {
                Serial.println("    [CaptivePortal Scan] Starting scan...");
                int n = WiFi.scanNetworks();
                
                // Store results
                numScanResults = min(n, MAX_WIFI_SCAN_RESULTS);
                for (int i = 0; i < numScanResults; i++) {
                    strncpy(scanResults[i].ssid, WiFi.SSID(i).c_str(), 32);
                    scanResults[i].ssid[31] = '\0';
                    scanResults[i].rssi = WiFi.RSSI(i);
                    scanResults[i].isConnected = false;
                }
                
                isWifiScanning = false;
                Serial.print("    [CaptivePortal Scan] Found ");
                Serial.print(numScanResults);
                Serial.println(" networks");
                
                // Send response
                if (pendingRequest) {
                    pendingRequest->send(200, "application/json", "{\"status\":\"ok\",\"count\":" + String(numScanResults) + "}");
                    pendingRequest = nullptr;
                }
                
                vTaskDelete(nullptr);
            },
            "wifi_scan_portal",
            4096,
            nullptr,
            1,
            nullptr,
            0  // Core 0
        );
        });
        
        // Catch-all for captive portal redirect
        captiveServer.onNotFound([](AsyncWebServerRequest *request) {
        request->redirect("http://demi.connect/");
        });

        captivePortalRoutesRegistered = true;
    }
    
    // Start server
    captiveServer.begin();
    
    // Start DNS server - redirect all queries to ESP32
    dnsServer.setTTL(3600);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", WiFi.softAPIP());
    
    captivePortalRunning = true;
    Serial.println("[CaptivePortal] Web server started on http://demi.connect");
    Serial.println("[CaptivePortal] DNS server started - redirects all domains to 192.168.4.1");
}

// Stop captive portal
void stopCaptivePortal() {
    if (!captivePortalRunning) {
        return;
    }
    Serial.println("[CaptivePortal] Stopping web server...");
    dnsServer.stop();
    captiveServer.end();
    captivePortalRunning = false;
}

#endif // CAPTIVE_PORTAL_H
