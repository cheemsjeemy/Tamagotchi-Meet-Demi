#ifndef RTC_HANDLER_H
#define RTC_HANDLER_H

#include <Arduino.h>
#include <RTClib.h>

extern RTC_DS3231 rtc;

bool initRTC();
void updateTimeFromRTC();
time_t getUnixTime();
void syncRTCFromUnixTime(time_t unixTime);
void setRTCTime(int year, int month, int day, int hour, int minute, int second);

#endif