#ifndef WEB_PAGES_H
#define WEB_PAGES_H

#include <Arduino.h>

const char INDEX_HTML[] PROGMEM = R"rawliteral(
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


#endif