#ifndef DEMI_HANDLER_H
#define DEMI_HANDLER_H

#include <Arduino.h>
#include <U8g2lib.h>
#include <Preferences.h>
#include "demi_mood_enums.h"

// Personality Types
enum PersonalityType {
    PERS_NONE,
    PERS_ATTENTIVE,
    PERS_NEGLECTFUL,
    PERS_SPORADIC,
    PERS_ROUTINE
};

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

// ==============================================
// DEMI AI ARCHITECTURE - LAYERED SYSTEM
// STATS → DRIVES → MOOD ENGINE → AI MODIFIER → OUTPUT
// ==============================================
// DEMI AI ARCHITECTURE - LAYERED SYSTEM
// STATS → DRIVES → MOOD ENGINE → AI MODIFIER → OUTPUT
// ==============================================

// LAYER 1: BASE STATS (GROUND TRUTH)
// These are the only "real" values, driven purely by system time and user actions
struct DemiStats {
    int hunger;
    int happiness;
    int energy;
    int health;
    int cleanliness;
    bool isSleeping;
};

// LAYER 2: DRIVES (NEEDS CALCULATED FROM STATS)
// Translates raw stats into motivational drives
struct DemiDrives {
    float needFood;       // 0.0 - 1.0
    float needRest;       // 0.0 - 1.0
    float needClean;      // 0.0 - 1.0
    float needAttention;  // 0.0 - 1.0
    float discomfort;     // Composite drive score
};

// LAYER 3: MOOD ENGINE (RULE BASED, DETERMINISTIC)
// Calculates base mood scores entirely from drives - NO AI HERE
struct DemiMoodScores {
    float happy;      // 0.0 - 1.0
    float hungry;     // 0.0 - 1.0
    float sad;        // 0.0 - 1.0
    float tired;      // 0.0 - 1.0
    float dirty;      // 0.0 - 1.0
    float neutral;    // 0.0 - 1.0
    float excited;    // 0.0 - 1.0
    float concerned;  // 0.0 - 1.0
    float spooked;    // 0.0 - 1.0
};

// LAYER 4: TINYML AI MODIFIER (ONLY A NUDGE)
// AI ONLY outputs small bias values - NEVER decides mood directly
struct AIBias {
    float mood_bias[3];    // [-1.0 .. 1.0] happy, hungry, sad
    float urgency_boost;   // [-0.03 .. 0.03] - subtle need intensity variation
    float emotional_noise; // [-0.03 .. 0.03] - tiny mood personality variation
};

// TFLite TinyML Modifier
class DemiMoodModel {
public:
    DemiMoodModel();
    AIBias calculateBias(float hunger, float happiness, float energy, float health, float cleanliness);
    bool isReady();
    bool usingNeuralNetwork() { return useNeuralNetwork; }

private:
    bool initialized;
    bool useNeuralNetwork;
};

// AI state tracking
struct DemiAI {
    int consecutiveCares = 0;
    int neglectCount = 0;
    int totalCares = 0;
    unsigned long lastCareTime = 0;
    PersonalityType personality = PERS_NONE;
    int attentionScore = 0;
    int routineScore = 0;
    AIBias lastBias;
    struct {
        unsigned long avgCareInterval;
        int confidence;
        bool anticipationMode;
    } prediction;
};

extern DemiMoodModel moodModel;

// Constants for frame delays and sprite dimensions
#define IDLE_FRAME_DELAY 300
#define ALERT_FRAME_DELAY 150
#define SPRITE_WIDTH 128
#define SPRITE_HEIGHT 64

// ==============================================
// STAT DROP TIMING CONFIGURATION
// ==============================================
// Target: Stats deplete to ~15-20% in ~12 hours
// Base interval: 20 seconds (slower than 18s for better pacing)
#define STAT_UPDATE_INTERVAL_MS 20000     // Main timer interval (20 seconds)

// Drop intervals - staggered for natural feel
// All stats drop at similar rates (~12-hour cycle)
// Calculation: (20s × interval × 100 drops) / 3600 = hours to deplete
// Offset ensures they don't all sync up
#define HUNGER_DROP_INTERVAL 24           // Every 480 sec (8 min) = ~7.5 pts/hr → 12-13 hrs
#define ENERGY_DROP_INTERVAL 26           // Every 520 sec (8.7 min) = ~6.9 pts/hr → 13-14 hrs  
#define HAPPINESS_DROP_INTERVAL 28        // Every 560 sec (9.3 min) = ~6.4 pts/hr → 14-15 hrs
#define CLEANLINESS_DROP_INTERVAL 25      // Every 500 sec (8.3 min) = ~7.2 pts/hr → 13 hrs

// Drop offsets to stagger depletion (prevents all stats dropping at once)
#define HUNGER_DROP_OFFSET 0
#define ENERGY_DROP_OFFSET 12
#define HAPPINESS_DROP_OFFSET 6
#define CLEANLINESS_DROP_OFFSET 18

// Drop amounts per tick
#define HUNGER_DROP_AMOUNT 1
#define HAPPINESS_DROP_AMOUNT 1
#define ENERGY_DROP_AMOUNT 1
#define CLEANLINESS_DROP_AMOUNT 1
#define HEALTH_DROP_AMOUNT 1

// Function declarations
const unsigned char* getCurrentSprite();
int getFrameCount();
int getFrameDelay();
void setState(SystemState newState);
void updateStats();
void drawSpriteWithStats(U8G2_SH1106_128X64_NONAME_F_HW_I2C& u8g2, const unsigned char* sprite);
void updateDemi(U8G2_SH1106_128X64_NONAME_F_HW_I2C& u8g2);

// Stats functions
extern int hunger;
extern int happiness;
extern int energy;
extern int health;
extern int cleanliness;
extern bool isSleeping;
extern bool rtcTimeAvailable;
extern time_t lastSaveTime;
extern time_t lastPluggedIn;
extern time_t sleepStartTime;
extern int startEnergy;
extern int sleepDebtHealth;
extern DemiAI ai;

// Layer functions
DemiDrives calculateDrives(const DemiStats& stats);
DemiMoodScores calculateBaseMood(const DemiDrives& drives);
DemiMoodScores applyAIBias(DemiMoodScores baseMood, const AIBias& bias);
DemiMood resolveFinalMood(const DemiMoodScores& scores);

void saveAll();
void loadAll();
DemiMood getDemiMood();
const char* getMoodString(DemiMood mood);
void onCareAction();
void checkNeglect();

#endif // DEMI_HANDLER_H