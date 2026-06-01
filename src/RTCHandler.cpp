#include "RTCHandler.h"
#include "DemiHandler.h"  // for rtcTimeAvailable

RTC_DS3231 rtc;
bool rtcInitialized = false;
time_t lastRTCTime = 0;
extern bool rtcTimeAvailable;

bool initRTC() {
    delay(50); // Small delay to ensure I2C bus is ready
    
    // Probe I2C bus for RTC at address 0x68 (DS3231 default)
    Wire.beginTransmission(0x68);
    uint8_t error = Wire.endTransmission();
    
    if (error != 0) {
        Serial.printf("[RTC] No RTC found at 0x68 (error: %d)\n", error);
        rtcTimeAvailable = false;
        rtcInitialized = false;
        return false;
    }
    
    if (!rtc.begin()) {
        Serial.println("[RTC] rtc.begin() failed!");
        rtcTimeAvailable = false;
        rtcInitialized = false;
        return false;
    }
    
    rtcInitialized = true;
    
    if (rtc.lostPower()) {
        Serial.println("[RTC] RTC lost power, need to set time!");
        rtc.adjust(DateTime(2024, 1, 1, 0, 0, 0));
    }
    
    DateTime now = rtc.now();
    // Add 8 hours for Philippines timezone (UTC+8)
    int hour = now.hour() + 8;
    int day = now.day();
    int month = now.month();
    int year = now.year();
    
    if (hour >= 24) {
        hour -= 24;
        day++;
        if (day > 31 || (month == 2 && day > 28) || ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30)) {
            day = 1;
            month++;
            if (month > 12) {
                month = 1;
                year++;
            }
        }
    }
    
    Serial.printf("[RTC] Time: %04d-%02d-%02d %02d:%02d:%02d\n",
        year, month, day, hour, now.minute(), now.second());
    
    // Validate RTC
    if (!rtc.lostPower() && year >= 2025) {
        rtcTimeAvailable = true;
        Serial.println("[RTC] Time is valid");
    } else {
        rtcTimeAvailable = false;
        Serial.println("[RTC] Time invalid, will try NTP sync");
    }
    
    return true;
}

void updateTimeFromRTC() {
    if (!rtcInitialized) return;
    if (rtc.lostPower()) {
        Serial.println("[RTC] Power lost, time may be incorrect!");
    }
}

time_t getUnixTime() {
    if (!rtcInitialized) {
        if (!rtc.begin()) return 0;
        rtcInitialized = true;
    }
    
    // Check if RTC is responding by attempting a simple read first
    DateTime now = rtc.now();
    
    // Validate the returned time (year should be reasonable)
    if (now.year() < 2020 || now.year() > 2100) {
        // Invalid time read, RTC might not be responding properly
        static unsigned long lastRTCError = 0;
        if (millis() - lastRTCError > 5000) { // Limit error messages to every 5 seconds
            Serial.println("[RTC] Warning: Invalid time read, RTC may not be responding");
            lastRTCError = millis();
        }
        rtcTimeAvailable = false;
        return 0;
    }
    
    return now.unixtime();
}

void syncRTCFromUnixTime(time_t unixTime) {
    if (!rtcInitialized) return;
    if (unixTime == 0) return;
    
    DateTime dt(unixTime);
    rtc.adjust(dt);
    lastRTCTime = unixTime;
    Serial.printf("[RTC] Synced from NTP: %04d-%02d-%02d %02d:%02d:%02d\n",
        dt.year(), dt.month(), dt.day(),
        dt.hour(), dt.minute(), dt.second());
}

void setRTCTime(int year, int month, int day, int hour, int minute, int second) {
    if (!rtcInitialized) return;
    rtc.adjust(DateTime(year, month, day, hour, minute, second));
    Serial.printf("[RTC] Time set to: %04d-%02d-%02d %02d:%02d:%02d\n",
        year, month, day, hour, minute, second);
}

bool isRTCValid() {
    if (!rtcInitialized) return false;
    if (rtc.lostPower()) return false;
    DateTime now = rtc.now();
    if (now.year() < 2025) return false; // invalid year (we are in 2026)
    return true;
}