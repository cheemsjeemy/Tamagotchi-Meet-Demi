# Introduction to Demi

Demi is a cat. Demi is a he. Demi is a companion travel-gotchi that always has your back and is here to make you smile even if your world is upside down.
---

## Features (For now... C: )

### 1. **Menu System**
Demi includes a dynamic menu system that allows users to interact with the device through various options. The menu is organized into folders and actions, making it easy to navigate and control the system. Key menu categories include:
- **Settings**: Adjust brightness, view device information, and save preferences.
- **Demi**: Interactive actions like petting, jumping, washing, and feeding.
- **WiFi**: Manage WiFi connections, scan for networks, and connect to saved networks.
- **Bluetooth**: Toggle Bluetooth, set the device name, and test Bluetooth functionality.
- **MSAuth**: Set up and display TOTP codes for Microsoft Authenticator.

---

### 2. **WiFi Management**
Demi provides robust WiFi management capabilities, including:
- Scanning for available networks.
- Connecting to saved networks.
- Managing saved networks with options to forget or auto-connect.
- Displaying connection status and network details.

---

### 3. **Bluetooth Connectivity**
Demi supports Bluetooth functionality, allowing users to:
- Enable or disable Bluetooth.
- Set a custom device name.
- Test Bluetooth connectivity.

---

### 4. **TOTP (Time-based One-Time Password)**
Demi includes TOTP functionality for secure authentication. Features include:
- Generating TOTP codes based on a Base32 secret.
- Displaying the TOTP secret for setup in Microsoft Authenticator.
- Synchronizing time with an NTP server to ensure accurate TOTP generation.

---

### 5. **Preferences and Settings**
Demi allows users to save and load settings using the ESP32's preferences library. This includes:
- WiFi and Bluetooth toggles.
- Brightness levels.
- Saved WiFi networks and their credentials.

---

### 6. **Interactive Actions**
Demi includes fun and interactive actions, such as:
- Petting and jumping in the "Play" menu.
- Washing and feeding in the "Demi" menu.
- Feeding options include various food items like apples, bananas, and strawberries.

---

## Technical Details

### 1. **Hardware**
Demi is built on the ESP32 platform, leveraging its capabilities for WiFi, Bluetooth, and general-purpose input/output (GPIO) control.

### 2. **Software**
The project is written in C++ and utilizes the following libraries:
- **Arduino Core for ESP32**: Provides the foundation for the project.
- **ESPAsyncWebServer**: Enables the captive portal for WiFi configuration.
- **TOTP**: Implements the TOTP functionality.
- **U8G2**: Handles OLED display rendering for the menu and QR codes.

### 3. **Menu Structure**
The menu system is implemented using a tree-like structure, where each menu item is linked to its parent, siblings, and children. This allows for dynamic navigation and interaction.

---
