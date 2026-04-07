#include "DemiHandler.h"
#include <Arduino.h>
#include <U8g2lib.h>
#include "sprite_idle.h"
#include "sprite_alert.h"
#include "menu.h"
#include "sprite_Demi_Cosmetics.h"


// Define RGB LED pin
#define RGB_LED_PIN 48

// Demi's stats
int hunger = 100;
int happiness = 100;
int energy = 100;
int health = 100;
int cleanliness = 100;

// System state
SystemState currentState = STATE_IDLE;

// Animation state
AnimationState animState = ANIM_IDLE;
unsigned long lastFrameTime = 0;
int currentFrame = 0;

// Number of frames per animation
const int IDLE_FRAMES = 2;
const int ALERT_FRAMES = 3;

// Function to get current sprite frame based on animation state
const unsigned char* getCurrentSprite() {
    if (animState == ANIM_IDLE) {
        return (currentFrame == 0) ? IDLE_1 : IDLE_2;
    } else {
        return (currentFrame == 0) ? ALERT_1 : (currentFrame == 1) ? ALERT_2 : ALERT_3;
    }
}

// Function to get frame count for current animation
int getFrameCount() {
    return (animState == ANIM_IDLE) ? IDLE_FRAMES : ALERT_FRAMES;
}

// Function to get frame delay for current animation
int getFrameDelay() {
    return (animState == ANIM_IDLE) ? IDLE_FRAME_DELAY : ALERT_FRAME_DELAY;
}

// Change system state
void setState(SystemState newState) {
    if (currentState != newState) {
        currentState = newState;

        // Update LED color based on system state
        if (newState == STATE_IDLE) {
            animState = ANIM_IDLE;
            neopixelWrite(RGB_LED_PIN, 0, 50, 0); // Green for idle
        } else if (newState == STATE_MENU) {
            neopixelWrite(RGB_LED_PIN, 0, 25, 50); // Cyan for menu
            initMenu(); // Initialize menu when entering
        } else if (newState == STATE_ALERT) {
            animState = ANIM_ALERT;
            neopixelWrite(RGB_LED_PIN, 50, 0, 0); // Red for alert
        }

        // Reset animation frame
        currentFrame = 0;
        lastFrameTime = millis();
    }
}

// Update Demi's stats over time
void updateStats() {
    static unsigned long lastUpdate = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastUpdate >= 1000) { // Update every second
        lastUpdate = currentTime;

        // Stats depletion currently disabled
        // hunger = max(0, hunger - 1);
        // happiness = max(0, happiness - 1);
        // energy = max(0, energy - 1);
        // cleanliness = max(0, cleanliness - 1);
    }
}

// Draw sprite with Demi's stats
void drawSpriteWithStats(U8G2_SH1106_128X64_NONAME_F_HW_I2C& u8g2, const unsigned char* sprite) {
    u8g2.clearBuffer();

    // Draw sprite first
    u8g2.drawBitmap(0, 0, SPRITE_WIDTH / 8, SPRITE_HEIGHT, sprite);

    u8g2.setFont(u8g2_font_6x10_tf);
    
    // TOP LEFT CORNER: Energy, Hunger
    // Energy
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    u8g2.drawGlyph(0, 10, 0x0060);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(10, 10);
    u8g2.print(energy);
    
    // Hunger
    u8g2.drawBitmap(0, 12, 1, 8, hungericon);
    u8g2.setCursor(10, 20);
    u8g2.print(hunger);

    // TOP CENTER: Health
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    u8g2.drawGlyph(52, 10, 0x00b7); // Center alignment (128/2 - 8 = 56)
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(62, 10);
    u8g2.print(health);

    // TOP RIGHT CORNER: Happiness, Cleanliness
    // Happiness
    u8g2.drawBitmap(98, 2, 1, 8, happinessicon);
    u8g2.setCursor(108, 10);
    u8g2.print(happiness);
    
    // Cleanliness
    u8g2.setFont(u8g2_font_open_iconic_all_1x_t);
    u8g2.drawGlyph(98, 20, 0x0098);
    u8g2.setFont(u8g2_font_6x10_tf);
    u8g2.setCursor(108, 20);
    u8g2.print(cleanliness);

    u8g2.sendBuffer();
}

// Update Demi's animation and stats
void updateDemi(U8G2_SH1106_128X64_NONAME_F_HW_I2C& u8g2) {
    unsigned long currentTime = millis();

    // Update animation frame
    if (currentTime - lastFrameTime >= (unsigned long)getFrameDelay()) {
        lastFrameTime = currentTime;
        currentFrame++;
        if (currentFrame >= getFrameCount()) {
            currentFrame = 0;
        }
    }

    // Update stats and draw sprite with stats
    updateStats();
    drawSpriteWithStats(u8g2, getCurrentSprite());
}