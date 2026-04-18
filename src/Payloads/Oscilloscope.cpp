#include "Oscilloscope.h"
#include <U8g2lib.h>

volatile bool oscilloscopeActive = false;
volatile bool oscilloscopeRunning = false;

static uint16_t sampleBuffer[NUM_SAMPLES];
static volatile uint8_t sampleIndex = 0;
static hw_timer_t* sampleTimer = nullptr;
static portMUX_TYPE timerMux = portMUX_INITIALIZER_UNLOCKED;

static float voltageDivider = 2.0f;
static bool oscilloscopeDrawn = false;
static bool hasSignal = false;

#define SAMPLE_RATE_US 500
#define NOISE_THRESHOLD 100
#define SIGNAL_THRESHOLD 800

void IRAM_ATTR sampleISR() {
    if (oscilloscopeRunning && sampleIndex < NUM_SAMPLES) {
        uint32_t sum = 0;
        for(int i = 0; i < 8; i++) sum += analogRead(OSCILLOSCOPE_PIN);
        sampleBuffer[sampleIndex++] = sum >> 3;
    } else if (sampleIndex >= NUM_SAMPLES) {
        sampleIndex = 0;
    }
}

void initOscilloscope() {
    Serial.println("[Oscilloscope] Initialized on GPIO 13 (ADC)");
}

void startOscilloscope() {
    if (oscilloscopeRunning) return;
    
    sampleIndex = 0;
    memset(sampleBuffer, 0, NUM_SAMPLES);
    
    sampleTimer = timerBegin(0, 80, true);
    timerAttachInterrupt(sampleTimer, &sampleISR, true);
    timerAlarmWrite(sampleTimer, SAMPLE_RATE_US, true);
    timerAlarmEnable(sampleTimer);
    
    oscilloscopeRunning = true;
    oscilloscopeActive = true;
    oscilloscopeDrawn = false;
    Serial.println("[Oscilloscope] Started sampling");
}

void stopOscilloscope() {
    if (sampleTimer) {
        timerDetachInterrupt(sampleTimer);
        timerEnd(sampleTimer);
        sampleTimer = nullptr;
    }
    oscilloscopeRunning = false;
    oscilloscopeActive = false;
    Serial.println("[Oscilloscope] Stopped");
}

void adjustVoltageDivider(float delta) {
    voltageDivider += delta;
    if (voltageDivider < 1.0f) voltageDivider = 1.0f;
    if (voltageDivider > 10.0f) voltageDivider = 10.0f;
    Serial.printf("[Oscilloscope] Voltage divider: %.1fx\n", voltageDivider);
}

void updateOscilloscope(U8G2& u8g2, bool leftPressed, bool rightPressed, bool leftJustPressed, bool rightJustPressed) {
    if (leftJustPressed) adjustVoltageDivider(-0.5f);
    if (rightJustPressed) adjustVoltageDivider(0.5f);
    
    if (!oscilloscopeActive) {
        if (oscilloscopeDrawn) {
            oscilloscopeDrawn = false;
        }
        return;
    }
    
    u8g2.clearBuffer();
    u8g2.setDrawColor(1);
    
    u8g2.drawFrame(0, 0, 128, 64);
    u8g2.drawLine(64, 0, 64, 64);
    u8g2.drawLine(0, 32, 128, 32);
    
    uint8_t yMin = 14;
    uint8_t yMax = 54;
    uint8_t yRange = yMax - yMin;
    
    float maxVoltage = 3300 * voltageDivider;
    
    uint8_t currentIndex = sampleIndex;
    
    uint16_t minVal = 4095;
    uint16_t maxVal = 0;
    for (int i = 0; i < NUM_SAMPLES; i++) {
        if (sampleBuffer[i] < minVal) minVal = sampleBuffer[i];
        if (sampleBuffer[i] > maxVal) maxVal = sampleBuffer[i];
    }
    
    float displayMin = minVal * maxVoltage / 4095;
    float displayMax = maxVal * maxVoltage / 4095;
    float range = displayMax - displayMin;
    
    if (range < SIGNAL_THRESHOLD) {
        u8g2.setFont(u8g2_font_5x8_tr);
        u8g2.drawStr(40, 32, "NO SIGNAL");
    } else {
        for (int i = 0; i < NUM_SAMPLES - 1; i++) {
            uint8_t x1 = i;
            uint8_t x2 = i + 1;
            uint16_t v1 = sampleBuffer[i] * maxVoltage / 4095;
            uint16_t v2 = sampleBuffer[i + 1] * maxVoltage / 4095;
            uint8_t y1 = yMin + yRange - ((v1 - displayMin) * yRange / range);
            uint8_t y2 = yMin + yRange - ((v2 - displayMin) * yRange / range);
            u8g2.drawLine(x1, y1, x2, y2);
        }
    }
    
    uint16_t rawVoltage = currentIndex > 0 ? sampleBuffer[currentIndex - 1] : 0;
    uint16_t realVoltage = rawVoltage * maxVoltage / 4095;
    char status[24];
    snprintf(status, sizeof(status), "%d mV", realVoltage);
    u8g2.setFont(u8g2_font_5x8_tr);
    u8g2.drawStr(2, 8, status);
    
    char ratioStr[16];
    snprintf(ratioStr, sizeof(ratioStr), "x%.1f", voltageDivider);
    u8g2.drawStr(50, 8, ratioStr);
    
    u8g2.drawStr(90, 8, "RB:Exit");
    
    u8g2.setFont(u8g2_font_4x6_tr);
    snprintf(ratioStr, sizeof(ratioStr), "%.0fmV", displayMin);
    u8g2.drawStr(2, 60, ratioStr);
    snprintf(ratioStr, sizeof(ratioStr), "%.0fmV", displayMax);
    u8g2.drawStr(110, 60, ratioStr);
    
    u8g2.drawStr(2, 50, "< >:Adjust");
    
    u8g2.sendBuffer();
    oscilloscopeDrawn = true;
}

void cbOscilloscope() {
    Serial.println("[Oscilloscope] Menu action triggered");
    startOscilloscope();
}