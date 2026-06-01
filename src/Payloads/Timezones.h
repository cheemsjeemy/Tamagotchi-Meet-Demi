#ifndef TIMEZONES_H
#define TIMEZONES_H

#include <Arduino.h>
#include <U8g2lib.h>
#include "RTCHandler.h"

struct TimezoneInfo {
    const char* name;
    const char* countryCode;
    int offsetMinutes;
};

extern const TimezoneInfo timezones[];
extern const int NUM_TZ;
extern String LocalTimeZone;
extern bool timezonesActive;

void initTimezones();
void startTimezones();
void stopTimezones();
void updateTimezones(U8G2& u8g2, bool leftPressed, bool rightPressed, bool upPressed, bool downPressed, bool leftJustPressed, bool rightJustPressed, bool upJustPressed, bool downJustPressed);
extern void cbTimezones();

#endif // TIMEZONES_H
