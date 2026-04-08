#ifndef CAPTIVE_PORTAL_H
#define CAPTIVE_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <freertos/FreeRTOS.h>
#include <esp_task.h>
#include "menu.h"

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

static bool captivePortalRoutesRegistered = false;
static bool captivePortalRunning = false;

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
        String html = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Connect Demi to WiFi</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            max-width: 400px;
            margin: 40px auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
        }
        h1 {
            color: #333;
            margin-bottom: 10px;
            font-size: 24px;
        }
        p {
            color: #666;
            margin-bottom: 25px;
            font-size: 14px;
        }
        label {
            display: block;
            margin-bottom: 8px;
            color: #333;
            font-weight: 600;
            font-size: 14px;
        }
        select, input {
            width: 100%;
            padding: 14px;
            margin-bottom: 20px;
            border: 2px solid #e0e0e0;
            border-radius: 10px;
            font-size: 16px;
            box-sizing: border-box;
            transition: border-color 0.3s;
        }
        select:focus, input:focus {
            outline: none;
            border-color: #667eea;
        }
        button {
            width: 100%;
            padding: 16px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: 600;
            cursor: pointer;
            transition: transform 0.2s, box-shadow 0.2s;
        }
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 10px 30px rgba(102, 126, 234, 0.4);
        }
        button:disabled {
            opacity: 0.6;
            cursor: not-allowed;
        }
        .loading {
            display: none;
            text-align: center;
            padding: 20px;
        }
        .spinner {
            border: 4px solid #f3f3f3;
            border-top: 4px solid #667eea;
            border-radius: 50%;
            width: 40px;
            height: 40px;
            animation: spin 1s linear infinite;
            margin: 0 auto 15px;
        }
        @keyframes spin {
            0% { transform: rotate(0deg); }
            100% { transform: rotate(360deg); }
        }
        .status {
            margin-top: 20px;
            padding: 15px;
            border-radius: 10px;
            text-align: center;
            font-weight: 600;
        }
        .status.success {
            background: #d4edda;
            color: #155724;
        }
        .status.error {
            background: #f8d7da;
            color: #721c24;
        }
    </style>
</head>
<body>
    <div class="container">
        <h1>🌟 Connect Demi</h1>
        <p>Enter your WiFi credentials to connect Demi to the internet.</p>
        
        <form id="wifiForm">
            <label for="ssid">Select Your WiFi:</label>
            <select id="ssid" name="ssid">
                <option value="">-- Tap 'Refresh' to scan --</option>
            </select>
            
            <label for="password">WiFi Password:</label>
            <input type="password" id="password" name="password" placeholder="Enter password">
            
            <button type="button" id="refreshBtn" onclick="refreshNetworks()">Refresh Networks</button>
            <button type="submit" id="connectBtn">Connect</button>
        </form>
        
        <div class="loading" id="loading">
            <div class="spinner"></div>
            <p>Connecting Demi to WiFi...</p>
        </div>
        
        <div class="status" id="status" style="display: none;"></div>
    </div>
    
    <script>
        // Load networks on page load
        window.addEventListener('load', function() {
            refreshNetworks();
        });
        
        // Refresh networks function
        function refreshNetworks() {
            const select = document.getElementById('ssid');
            select.innerHTML = '<option value="">-- Scanning... --</option>';
            
            fetch('/networks')
                .then(response => response.json())
                .then(networks => {
                    select.innerHTML = '';
                    if (networks.length === 0) {
                        select.innerHTML = '<option value="">-- No networks found --</option>';
                        return;
                    }
                    networks.forEach(network => {
                        const option = document.createElement('option');
                        option.value = network.ssid;
                        option.textContent = network.ssid + ' (' + network.rssi + '%)';
                        select.appendChild(option);
                    });
                })
                .catch(err => {
                    select.innerHTML = '<option value="">-- Error loading --</option>';
                    console.error('Error loading networks:', err);
                });
        }
        
        document.getElementById('wifiForm').addEventListener('submit', async function(e) {
            e.preventDefault();
            
            const ssid = document.getElementById('ssid').value;
            const password = document.getElementById('password').value;
            
            if (!ssid) {
                alert('Please select a WiFi network');
                return;
            }
            
            document.getElementById('wifiForm').style.display = 'none';
            document.getElementById('loading').style.display = 'block';
            
            try {
                const response = await fetch('/connect', {
                    method: 'POST',
                    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
                    body: 'ssid=' + encodeURIComponent(ssid) + '&password=' + encodeURIComponent(password)
                });
                
                const result = await response.json();
                
                document.getElementById('loading').style.display = 'none';
                document.getElementById('status').style.display = 'block';
                
                if (result.success) {
                    document.getElementById('status').className = 'status success';
                    document.getElementById('status').innerHTML = '✓ Connected!<br>Demi is now online.<br>IP: ' + result.ip;
                } else {
                    document.getElementById('status').className = 'status error';
                    document.getElementById('status').textContent = 'Error: ' + result.message;
                    document.getElementById('wifiForm').style.display = 'block';
                }
            } catch (err) {
                document.getElementById('loading').style.display = 'none';
                document.getElementById('status').style.display = 'block';
                document.getElementById('status').className = 'status error';
                document.getElementById('status').textContent = 'Connection error';
                document.getElementById('wifiForm').style.display = 'block';
            }
        });
    </script>
</body>
</html>
)rawliteral";
        
        request->send(200, "text/html", html);
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
        request->redirect("/");
        });

        captivePortalRoutesRegistered = true;
    }
    
    // Start server
    captiveServer.begin();
    captivePortalRunning = true;
    Serial.println("[CaptivePortal] Web server started on http://192.168.4.1");
}

// Stop captive portal
void stopCaptivePortal() {
    if (!captivePortalRunning) {
        return;
    }
    Serial.println("[CaptivePortal] Stopping web server...");
    captiveServer.end();
    captivePortalRunning = false;
}

#endif // CAPTIVE_PORTAL_H
