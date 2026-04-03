# Tamagotchi Menu System - Technical Plan

## 1. Menu Structure (Tree Data Structure)

```
Root Menu (C:/)
├── Settings
│   ├── Brightness
│   │   └── Brightness Bar (Press Select first to enter edit, before pressing left or right to change the brightness, press Select to exit the brightness edit)
│   └── About
├── Demi (Pet name - Tamagotchi)
│   ├── Play
│   │   ├── Petting
│   │   └── Jumping
│   ├── Wash
│   ├── Feed
│   │   └── Fridge
│   │       └── Apple, etc. (future: food items)
│   └── Sleep
├── Wifi
│   ├── Wifi [X] (WiFi Toggle)
│   ├── Connections
│   │   ├── Device → ESP32
│   │   │    ├── Status: Connected / None
│   │   │    └── Name of the Device/s Connected
│   │   │
│   │   └── ESP32 → Network
│   │        ├── Status: Connected / Not Connected
│   │        ├── SSID: (WIFI NAME OF THE NETWORK/ROUTER)
│   │        └── IP: xxx.xxx.xxx.xxx (IP of ESP32 that was given by the Network/Router)
│   │
│   ├── Connect to Network
│   │   ├── Scan Networks
│   │   │   └── (WIFI NAME)
│   │   │        ├── Connect via Phone (QR)
│   │   │        └── Forget (if saved)
│   │   │
│   │   └── Saved Networks
│   │        └── (WIFI NAME)
│   │             ├── Connect
│   │             └── Forget
│   │
│   └── Device Access (Connect TO Demi)
│       ├── Show QR (Auto Connect Page)
│       └── Show SSID + Password
├── Bluetooth
└── MSAuthenticator
```

### Item Types:
- **Settings**: Folder → contains Brightness, About
- **Demi**: Folder → main pet menu
- **Play**: Folder → contains Petting, Jumping
- **Petting/Jumping**: Action (triggers sprite animation)
- **Wash**: Action (triggers wash animation)
- **Feed**: Folder → contains Fridge
- **Fridge**: Future - food items storage
- **Sleep**: Action (puts pet to sleep)
- **Wifi/Bluetooth**: Toggles (On/Off)
- **MSAuthenticator**: Placeholder action
- **Brightness**: Slider (0-255)

### Menu Item Types
- **FOLDER**: Opens sub-menu (has children)
- **ACTION**: Executes function on select
- **TOGGLE**: On/Off states (e.g., Wifi, Bluetooth)
- **SLIDER**: Adjustable value (e.g., Brightness 0-255)

## 2. Data Structure Design

```cpp
// Menu item types
enum MenuItemType {
MENU_FOLDER,    // Has sub-items
MENU_ACTION,    // Executes callback
MENU_TOGGLE,    // On/Off
MENU_SLIDER     // Adjustable value
};

struct MenuItem {
const char* name;           // Display name
MenuItemType type;          // Item type
MenuItem* parent;           // Parent menu (null = root)
MenuItem* firstChild;       // First child (if folder)
MenuItem* nextSibling;     // Next item at same level
MenuItem* prevSibling;     // Previous item at same level
void* value;                // For toggles/sliders
void (*callback)();        // For actions
};
```

## 3. Directory Path Display

```
┌────────────────────────┐
│ C:/Settings/Brightness │
├────────────────────────┤
│ ▶ Brightness           │  ← Selected (cursor)
│   Sleep Timeout        │
│   About                │
│                        │
└────────────────────────┘
```

- Format: `C:/` followed by current path
- Max ~20 chars on 128x64 display
- Scroll if path too long

## 4. Navigation Controls

### Menu Item Navigation
| Action | Input | Behavior |
|--------|-------|----------|
| Navigate Up | UP (release) | Select previous item |
| Navigate Down | DOWN (release) | Select next item |
| Select/Enter | CENTER (release) | Enter folder / Execute action |
| Decrease Value | LEFT (release) | Decrease slider/toggle value |
| Increase Value | RIGHT (release) | Increase slider/toggle value |
| Go Back | Double-tap LEFT | Return to parent menu |
| Enter Menu | DOWN + CENTER | Enter menu from idle |
| Exit to Idle | HOLD CENTER + UP | Return to sprite screen |

### Value Adjustment (Toggle/Slider)
| Action | Input | Behavior |
|--------|-------|----------|
| Toggle On/Off | LEFT or RIGHT (release) | Switch toggle state (for TRUE/FALSE items) |
| Decrease Value | LEFT (release) | Decrease slider value by step |
| Increase Value | RIGHT (release) | Increase slider value by step |
| EXIT EDIT MODE (FOR SLIDER) | CENTER (release) | Exit edit mode |

**Note:** All single-key actions fire on RELEASE (not press) to allow combo detection.
Combos are detected when the second key is pressed while first is held.

### Combo Key Detection
All combos are detected BEFORE single keys fire:
- If second key is pressed while first is held → combo triggers
- If no combo detected → single key fires on RELEASE

```cpp
// Track held keys
bool isKeyHeld(uint8_t pin, uint16_t threshold);

// Check combo: pressed together
bool isComboPressed(uint8_t pin1, uint8_t pin2);
```

## 5. State Machine

```
┌──────────┐     Down+CENTER      ┌──────────┐
│          │ ─────────────────▶  │          │
│   IDLE   │                     │   MENU   │
│ (Sprite) │ ◀─────────────────  │          │
└──────────┘   HOLD CENTER+UP    └──────────┘
```

States:
- **STATE_IDLE**: Sprite animation running
- **STATE_MENU**: Menu system active

## 6. Display Layout (128x64)

```
Line 0: C:/Settings/Brightness    (path - 8px font)
Line 1: ──────────────────────     (divider)
Line 2: ▶ Item One                (selected)
Line 3:   Item Two
Line 4:   Item Three
Line 5:   Item Four
Line 6:   Item Five
Line 7:   Item Six
```

- Font: 8px tall (u8g2_font_u8glib_4_hr)
- Line height: 8px
- Max visible items: 6 per screen
- Scrolling when >6 items

## 7. UI Elements

- **Cursor**: `▶` (0x25b6) on left of selected item
- **Folder Icon**: `▶` (hollow right triangle)
- **Action Icon**: `●` (solid circle)
- **Toggle ON**: `[✓]` or `ON`
- **Toggle OFF**: `[ ]` or `OFF`
- **Slider**: `[███████░░] 80%` - horizontal bar with percentage
- **Slider Active**: Value changes while holding LEFT/RIGHT

### Slider Visual
```
Brightness    [█████░░░░] 50%
```
- Left: Label (max 10 chars)
- Middle: Bar (≈80px wide, filled based on value)
- Right: Percentage text

## 8. Implementation Steps

1. Create `menu.h` with MenuItem struct and tree functions
2. Define static menu items (root and all sub-menus)
3. Add MENU state to state machine
4. Implement combo key detection
5. Write menu rendering function
6. Implement navigation logic
7. Add callback functions for each action
8. Test menu flow

## 9. Files to Create/Modify

- **New**: `src/menu.h` - Menu data structure and functions
- **Modify**: `src/main.cpp` - Add menu state, rendering, navigation
