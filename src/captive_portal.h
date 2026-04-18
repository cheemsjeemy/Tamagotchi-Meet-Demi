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
#define CAPTIVE_AP_PASSWORD ""  // Open network - no password

// Login credentials
#define PORTAL_USERNAME "Demi"
#define PORTAL_PASSWORD "demitri"

// Login HTML page
const char LOGIN_HTML[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>Sign In - Demi Network</title>
    <style>
        body {
            font-family: -apple-system, BlinkMacSystemFont, 'Segoe UI', Roboto, sans-serif;
            max-width: 400px;
            margin: 40px auto;
            padding: 20px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            min-height: 100vh;
            display: flex;
            align-items: center;
            justify-content: center;
        }
        .container {
            background: white;
            border-radius: 20px;
            padding: 30px;
            box-shadow: 0 20px 60px rgba(0,0,0,0.3);
            width: 100%;
        }
        h1 {
            color: #333;
            margin-bottom: 20px;
            text-align: center;
        }
        .logo {
            text-align: center;
            font-size: 48px;
            margin-bottom: 10px;
        }
        input {
            width: 100%;
            padding: 12px;
            margin: 8px 0;
            border: 2px solid #e0e0e0;
            border-radius: 10px;
            box-sizing: border-box;
            font-size: 16px;
        }
        input:focus {
            border-color: #667eea;
            outline: none;
        }
        button {
            width: 100%;
            padding: 14px;
            background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
            color: white;
            border: none;
            border-radius: 10px;
            font-size: 16px;
            font-weight: bold;
            cursor: pointer;
            margin-top: 10px;
        }
        button:hover {
            opacity: 0.9;
        }
        .note {
            text-align: center;
            color: #666;
            font-size: 12px;
            margin-top: 15px;
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="logo">✨</div>
        <h1>Sign In to Network</h1>
        <form id="loginForm">
            <input type="text" id="username" placeholder="Username" required>
            <input type="password" id="password" placeholder="Password" required>
            <button type="submit">Connect</button>
        </form>
        <div class="note">Ask Demi for login credentials</div>
    </div>
    <script>
        document.getElementById('loginForm').addEventListener('submit', function(e) {
            e.preventDefault();
            var user = document.getElementById('username').value;
            var pass = document.getElementById('password').value;
            
            fetch('/auth?user=' + encodeURIComponent(user) + '&pass=' + encodeURIComponent(pass))
                .then(r => r.json())
                .then(data => {
                    if (data.success) {
                        window.location.href = '/home';
                    } else {
                        alert('Invalid credentials');
                    }
                });
        });
    </script>
</body>
</html>
)rawliteral";

// Check if user is logged in
static bool isAuthenticated(AsyncWebServerRequest *request) {
    return request->hasHeader("Cookie") && request->getHeader("Cookie")->value().indexOf("loggedin=true") >= 0;
}

// Authenticate user
static bool authenticateUser(const String& username, const String& password) {
    return username == PORTAL_USERNAME && password == PORTAL_PASSWORD;
}

// Custom domain for captive portal
#define CAPTIVE_DOMAIN "demi.connect"
 
extern bool captivePortalRoutesRegistered;
extern bool captivePortalRunning;
extern DNSServer dnsServer;


// Start captive portal web server
inline void startCaptivePortal() {
    if (captivePortalRunning) {
        Serial.println("[CaptivePortal] Web server already running");
        return;
    }

    Serial.println("[CaptivePortal] Starting web server...");

    if (!captivePortalRoutesRegistered) {
        // Root page - Login page (first time)
        captiveServer.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/html", LOGIN_HTML);
        });
        
        // Captive portal detection endpoints - return 204 No Content to trigger browser login
        captiveServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(204, "text/plain", "");
        });
        captiveServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(204, "text/plain", "");
        });
        captiveServer.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(204, "text/plain", "");
        });
        captiveServer.on("/connecttest", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(204, "text/plain", "");
        });
        captiveServer.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(204, "text/plain", "Microsoft NCSI");
        });
        
        // Authentication endpoint
        captiveServer.on("/auth", HTTP_GET, [](AsyncWebServerRequest *request) {
            if (request->hasParam("user") && request->hasParam("pass")) {
                String user = request->getParam("user")->value();
                String pass = request->getParam("pass")->value();
                
                if (authenticateUser(user, pass)) {
                    Serial.println("[CaptivePortal] Login successful");
                    request->send(200, "application/json", "{\"success\":true}");
                } else {
                    Serial.println("[CaptivePortal] Login failed");
                    request->send(200, "application/json", "{\"success\":false}");
                }
            } else {
                request->send(400, "application/json", "{\"success\":false,\"message\":\"Missing params\"}");
            }
        });
        
        // Home page - only accessible if authenticated
        captiveServer.on("/home", HTTP_GET, [](AsyncWebServerRequest *request) {
            // Check for auth cookie or parameter
            bool authenticated = false;
            
            if (request->hasParam("auth")) {
                String auth = request->getParam("auth")->value();
                if (auth == "true") authenticated = true;
            }
            
            if (authenticated || isAuthenticated(request)) {
                request->send(200, "text/html", INDEX_HTML);
            } else {
                request->send(200, "text/html", LOGIN_HTML);
            }
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
        
        // Captive portal detection endpoints (Android/iOS/Windows)
        captiveServer.on("/generate_204", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/hotspot-detect.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/library/test/success.html", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/captive.apple.com", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/www.apple.com/library/test/success", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/fw在做什麼", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/connecttest", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->redirect("http://demi.connect/");
        });
        captiveServer.on("/ncsi.txt", HTTP_GET, [](AsyncWebServerRequest *request) {
            request->send(200, "text/plain", "Microsoft NCSI");
        });
        
        // Catch-all for captive portal redirect - redirect ALL unknown requests to login
        captiveServer.onNotFound([](AsyncWebServerRequest *request) {
            Serial.printf("[CaptivePortal] onNotFound triggered for: %s\n", request->url().c_str());
            // Direct send instead of redirect to avoid browser issues
            request->send(200, "text/html", LOGIN_HTML);
        });

        captivePortalRoutesRegistered = true;
    }
    
    // Start server
    captiveServer.begin();
    
    // Start DNS server - redirect all queries to ESP32
    dnsServer.setTTL(3600);
    dnsServer.setErrorReplyCode(DNSReplyCode::NoError);
    dnsServer.start(53, "*", WiFi.softAPIP());
    Serial.printf("[CaptivePortal] DNS server started on all interfaces. AP IP: %s\n", WiFi.softAPIP().toString().c_str());
    
    captivePortalRunning = true;
    Serial.println("[CaptivePortal] Web server started on http://demi.connect");
    Serial.println("[CaptivePortal] DNS server started - redirects all domains to 192.168.4.1");
}

// Stop captive portal
inline void stopCaptivePortal() {
    if (!captivePortalRunning) {
        return;
    }
    Serial.println("[CaptivePortal] Stopping web server...");
    dnsServer.stop();
    captiveServer.end();
    captivePortalRunning = false;
}

// Helper function to process DNS requests in main loop
void processCaptivePortalDNS();

#endif // CAPTIVE_PORTAL_H
