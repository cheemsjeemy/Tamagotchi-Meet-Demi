#include "RTCHandler.h"

RTC_DS3231 rtc;
bool rtcInitialized = false;
time_t lastRTCTime = 0;

bool initRTC() {
    delay(50); // Small delay to ensure I2C bus is ready
    
    if (!rtc.begin()) {
        Serial.println("[RTC] Couldn't find RTC!");
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
    return rtc.now().unixtime();
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