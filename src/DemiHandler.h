#ifndef DEMI_HANDLER_H
#define DEMI_HANDLER_H

#include <U8g2lib.h>

// System state (what mode the device is in)
enum SystemState {
    STATE_IDLE,     // Sprite animation running
    STATE_MENU,     // Menu system active
    STATE_ALERT     // Alert animation playing
};

// Animation state (which animation to play)
enum AnimationState {
    ANIM_IDLE,
    ANIM_ALERT
};

// Constants for frame delays and sprite dimensions
#define IDLE_FRAME_DELAY 300
#define ALERT_FRAME_DELAY 150
#define SPRITE_WIDTH 128
#define SPRITE_HEIGHT 64

// Function declarations
const unsigned char* getCurrentSprite();
int getFrameCount();
int getFrameDelay();
void setState(SystemState newState);
void updateStats();
void drawSpriteWithStats(U8G2_SH1106_128X64_NONAME_F_HW_I2C& u8g2, const unsigned char* sprite);
void updateDemi(U8G2_SH1106_128X64_NONAME_F_HW_I2C& u8g2);

#endif // DEMI_HANDLER_H