#include "Timezones.h"
#include <U8g2lib.h>
#include "RTCHandler.h"
#include <time.h>

const TimezoneInfo timezones[] = {
    // --- Priority / User Choice ---
    {"Manila",      "PH", 480},    // UTC+8:00 (PH First)

    // --- Northeast Asia ---
    {"Taipei",      "TW", 480},    // UTC+8:00
    {"Beijing",     "CN", 480},    // UTC+8:00

    {"Tokyo",       "JP", 540},    // UTC+9:00
    {"Seoul",       "KR", 540},    // UTC+9:00

    // --- Southeast Asia (Remaining) ---
    {"Jakarta",     "ID", 420},    // UTC+7:00
    {"Kuala Lumpur","MY", 480},    // UTC+8:00
    {"Singapore",   "SG", 480},    // UTC+8:00
    {"Bangkok",     "TH", 420},    // UTC+7:00
    {"Hanoi",       "VN", 420},    // UTC+7:00

    // --- South Asia ---
    {"New Delhi",   "IN", 330}     // UTC+5:30
};


const int NUM_TZ = sizeof(timezones) / sizeof(timezones[0]);
static int selectedIdx = 0;
static int scrollOffset = 0;
bool timezonesActive = false;
static unsigned long lastUpdate = 0;

String LocalTimeZone = "PH";


void initTimezones() {}

void startTimezones() {
    timezonesActive = true;
    selectedIdx = 0;
    scrollOffset = 0;
    lastUpdate = millis();
    Serial.println("[Timezones] Started");
}

void stopTimezones() {
    timezonesActive = false;
    Serial.println("[Timezones] Stopped");
}

void updateTimezones(U8G2& u8g2, bool leftPressed, bool rightPressed, bool upPressed, bool downPressed, bool leftJustPressed, bool rightJustPressed, bool upJustPressed, bool downJustPressed) {
    if (!timezonesActive) return;

    unsigned long now = millis();
    bool forceRedraw = (now - lastUpdate > 1000);
    if (forceRedraw) lastUpdate = now;

    if (upJustPressed) {
        selectedIdx = (selectedIdx - 1 + NUM_TZ) % NUM_TZ;
        if (selectedIdx < scrollOffset) scrollOffset = selectedIdx;
        if (selectedIdx > scrollOffset + 3) scrollOffset = selectedIdx - 3;
    }
    if (downJustPressed) {
        selectedIdx = (selectedIdx + 1) % NUM_TZ;
        if (selectedIdx < scrollOffset) scrollOffset = selectedIdx;
        if (selectedIdx > scrollOffset + 3) scrollOffset = selectedIdx - 3;
    }

    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(3, 7, "WORLD CLOCK");
    
    for (int i = 0; i < 4; i++) {
        int idx = scrollOffset + i;
        if (idx >= NUM_TZ) continue;
        
        int y = i * 14 + 12;
        
        if (idx > 0) {y += 2;} // Add extra spacing after the first entry (Philippines)
        
        bool isSelected = (idx == selectedIdx);
        
        if (isSelected) {
            u8g2.setDrawColor(1);
            u8g2.drawBox(2, y, 124, 12);
            u8g2.setDrawColor(0);
        } else {
            u8g2.setDrawColor(1);
        }
        
        u8g2.setFont(u8g2_font_5x8_tr);
        if (isSelected) u8g2.setDrawColor(0);

        char cityBuf[32];
        snprintf(cityBuf, sizeof(cityBuf), "%s (%s)", timezones[idx].name, timezones[idx].countryCode);
        u8g2.drawStr(4, y + 9, cityBuf);
        
        time_t t = getUnixTime();
        time_t local = t + (timezones[idx].offsetMinutes * 60);
        struct tm* tm = gmtime(&local);
        
        int h = tm->tm_hour;
        const char* ap = h >= 12 ? " PM" : " AM";
        if (h > 12) h -= 12;
        if (h == 0) h = 12;
        
        char buf[16];
        snprintf(buf, sizeof(buf), "%d:%02d:%02d%s", h, tm->tm_min, tm->tm_sec, ap);
        
        int w = u8g2.getStrWidth(buf);
        if (isSelected) u8g2.setDrawColor(0);
        u8g2.drawStr(124 - w, y + 9, buf);

             // If this is the Philippines (idx 0), draw a small separator line below it
        if (idx == 0) {
            u8g2.setDrawColor(1);
            // Draws a 40-pixel wide line centered under the first entry
            u8g2.drawHLine(44, y + 13, 40); 
        }

    }
    
    u8g2.setDrawColor(1);
    u8g2.setFont(u8g2_font_4x6_tr);
    u8g2.drawStr(67, 7, ("LOCAL TIME: " + LocalTimeZone).c_str());
    u8g2.drawHLine(0, 10, 128);
    
    u8g2.sendBuffer();
}

void cbTimezones() {
    startTimezones();
}