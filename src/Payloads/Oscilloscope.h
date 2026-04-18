#ifndef OSCILLOSCOPE_H
#define OSCILLOSCOPE_H

#include <Arduino.h>
#include <U8g2lib.h>

#define OSCILLOSCOPE_PIN 13

#define NUM_SAMPLES 128

extern volatile bool oscilloscopeActive;
extern volatile bool oscilloscopeRunning;

void initOscilloscope();
void startOscilloscope();
void stopOscilloscope();
void updateOscilloscope(U8G2& u8g2, bool leftPressed = false, bool rightPressed = false, bool leftJustPressed = false, bool rightJustPressed = false);
void adjustVoltageDivider(float delta);

extern void cbOscilloscope();

#endif