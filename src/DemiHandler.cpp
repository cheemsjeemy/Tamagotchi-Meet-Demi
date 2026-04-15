#include "DemiHandler.h"
#include "demi_mood_enums.h"
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

// Sleep state
bool isSleeping = false;

// AI state
DemiAI ai = {};
DemiMoodModel moodModel;  // Global - init at static init time (before Serial ready)

// ========== TINYML MOOD PREDICTOR ==========
#include "demi_mood_model.h"
#include <TensorFlowLite_ESP32.h>
#include "tensorflow/lite/micro/all_ops_resolver.h"
#include "tensorflow/lite/micro/micro_interpreter.h"
#include "tensorflow/lite/schema/schema_generated.h"
#include "tensorflow/lite/micro/micro_error_reporter.h"

// Global tensor arena
static uint8_t tensorArena[16 * 1024];

namespace {
    const tflite::Model* model;
    tflite::MicroInterpreter* interpreter;
    TfLiteTensor* input;
    TfLiteTensor* output;
}



DemiMoodModel::DemiMoodModel() : initialized(false), useNeuralNetwork(false) {
    Serial.println("[AI] Initializing Demi AI Mood System...");

    // Try to init neural network regardless of schema version
    const tflite::Model* checkModel = tflite::GetModel(demi_v3_tflite);
    Serial.printf("[AI] Model schema version: %d\n", checkModel->version());
    
    static tflite::MicroErrorReporter micro_error_reporter;
    tflite::ErrorReporter* error_reporter = &micro_error_reporter;
    
    static tflite::AllOpsResolver resolver;
    static tflite::MicroInterpreter static_interpreter(
        checkModel, resolver, tensorArena, sizeof(tensorArena), error_reporter);
    interpreter = &static_interpreter;
    
    if (interpreter->AllocateTensors() == kTfLiteOk) {
        input = interpreter->input(0);
        output = interpreter->output(0);
        initialized = true;
        useNeuralNetwork = true;
        Serial.println("[AI] TFLite neural network ready!");
    } else {
        Serial.println("[AI] AllocateTensors failed, using rule-based AI");
        initialized = true;
    }
    
    Serial.println("[AI] Model ready!");
}



bool DemiMoodModel::isReady() {
    return initialized;
}

// ==============================================
// LAYER 4: TINYML AI MODIFIER
// AI ONLY outputs bias values - NEVER decides mood directly
// ==============================================
AIBias DemiMoodModel::calculateBias(float hunger, float happiness, float energy, float health, float cleanliness) {
    AIBias bias = {};
    
    // Default zero bias - AI starts neutral
    bias.mood_bias[0] = 0.0f;  // happy
    bias.mood_bias[1] = 0.0f;  // hungry
    bias.mood_bias[2] = 0.0f;  // sad
    bias.urgency_boost = 0.0f;
    bias.emotional_noise = 0.0f;
    
    if (!initialized || !useNeuralNetwork || interpreter == nullptr) {
        // Fallback: tiny random bias when NN not available
        bias.emotional_noise = (random(100) - 50) / 500.0f; // ±0.1 range
        return bias;
    }
    
    // Normalize inputs for model
    input->data.f[0] = hunger / 100.0f;
    input->data.f[1] = happiness / 100.0f;
    input->data.f[2] = energy / 100.0f;
    input->data.f[3] = health / 100.0f;
    input->data.f[4] = cleanliness / 100.0f;
    
    TfLiteStatus invoke_status = interpreter->Invoke();
    
    if (invoke_status == kTfLiteOk) {
        // MODEL ONLY OUTPUTS BIAS VALUES - NOT FULL MOODS
        // Model outputs 5 values: [happy_bias, hungry_bias, sad_bias, urgency, noise]
        if (output->type == kTfLiteFloat32) {
            bias.mood_bias[0] = constrain(output->data.f[0], -1.0f, 1.0f);
            bias.mood_bias[1] = constrain(output->data.f[1], -1.0f, 1.0f);
            bias.mood_bias[2] = constrain(output->data.f[2], -1.0f, 1.0f);
            bias.urgency_boost = constrain(output->data.f[3], -0.2f, 0.2f);
            bias.emotional_noise = constrain(output->data.f[4], -0.1f, 0.1f);
        } else if (output->type == kTfLiteInt8) {
            bias.mood_bias[0] = constrain((float)output->data.int8[0] / 127.0f, -1.0f, 1.0f);
            bias.mood_bias[1] = constrain((float)output->data.int8[1] / 127.0f, -1.0f, 1.0f);
            bias.mood_bias[2] = constrain((float)output->data.int8[2] / 127.0f, -1.0f, 1.0f);
            bias.urgency_boost = constrain((float)output->data.int8[3] / 635.0f, -0.2f, 0.2f);
            bias.emotional_noise = constrain((float)output->data.int8[4] / 1270.0f, -0.1f, 0.1f);
        }
    }
    
    return bias;
}

// ==============================================
// LAYER 1 → LAYER 2: STATS → DRIVES
// Pure functional translation - no randomness, no AI
// ==============================================
DemiDrives calculateDrives(const DemiStats& stats) {
    DemiDrives drives = {};
    
    // Invert stats: lower stat value = higher drive need
    drives.needFood      = (100.0f - stats.hunger)      / 100.0f;
    drives.needRest      = (100.0f - stats.energy)      / 100.0f;
    drives.needClean     = (100.0f - stats.cleanliness) / 100.0f;
    drives.needAttention = (100.0f - stats.happiness)   / 100.0f;
    
    // Discomfort is weighted sum of all drives
    drives.discomfort = 
        drives.needFood      * 1.6f +  // ✅ Hunger now 1.6x weight
        drives.needRest      * 1.8f +  // ✅ Energy now 1.8x weight (highest)
        drives.needClean     * 1.0f +
        drives.needAttention * 0.7f;
    
    drives.discomfort = constrain(drives.discomfort / 4.6f, 0.0f, 1.0f);
    
    return drives;
}

// ==============================================
// LAYER 2 → LAYER 3: DRIVES → BASE MOOD
// 100% rule based, deterministic curves
// ==============================================
DemiMoodScores calculateBaseMood(const DemiDrives& drives) {
    DemiMoodScores mood = {};
    
    // ✅ HUNGER AND ENERGY NOW HAVE STRONG DIRECT INFLUENCE ON MOOD
    float hunger_happiness_hit = drives.needFood * 0.6f;
    float energy_happiness_hit = drives.needRest * 0.7f;
    
    // Base moods are inverse of drives
    mood.happy      = 1.0f - drives.discomfort - hunger_happiness_hit - energy_happiness_hit;
    mood.hungry     = drives.needFood * 1.3f;  // +30% hunger priority
    mood.tired      = drives.needRest * 1.5f;  // +50% energy priority (highest)
    mood.dirty      = drives.needClean;
    mood.sad        = drives.needAttention * 0.8f;
    
    // Neutral is the middle ground
    mood.neutral    = 0.5f - fabs(drives.discomfort - 0.5f);
    
    // Excited only when all needs are mostly met
    mood.excited    = max(0.0f, (1.0f - drives.discomfort) - 0.6f) * 2.5f;
    
    // Concerned when discomfort is moderate
    mood.concerned  = max(0.0f, drives.discomfort - 0.3f) * 1.4f;
    mood.concerned  = min(mood.concerned, 1.0f - drives.discomfort);
    
    // Spooked only at very high discomfort
    mood.spooked    = max(0.0f, drives.discomfort - 0.8f) * 5.0f;
    
    // Clamp all values
    mood.happy      = constrain(mood.happy,      0.0f, 1.0f);
    mood.hungry     = constrain(mood.hungry,     0.0f, 1.0f);
    mood.sad        = constrain(mood.sad,        0.0f, 1.0f);
    mood.excited    = constrain(mood.excited,    0.0f, 1.0f);
    mood.tired      = constrain(mood.tired,      0.0f, 1.0f);
    mood.dirty      = constrain(mood.dirty,      0.0f, 1.0f);
    mood.neutral    = constrain(mood.neutral,    0.0f, 1.0f);
    mood.excited    = constrain(mood.excited,    0.0f, 1.0f);
    mood.concerned  = constrain(mood.concerned,  0.0f, 1.0f);
    mood.spooked    = constrain(mood.spooked,    0.0f, 1.0f);
    
    return mood;
}

// ==============================================
// LAYER 3 → LAYER 4: APPLY AI MODIFIER
// AI ONLY ADDS SMALL BIAS - MAX 20-30% INFLUENCE
// ==============================================
DemiMoodScores applyAIBias(DemiMoodScores baseMood, const AIBias& bias) {
    DemiMoodScores finalMood = baseMood;
    
    // ✅ AI ONLY NUDGES - NEVER FULL CONTROL
    finalMood.happy   += bias.mood_bias[0] * 0.2f;
    finalMood.hungry  += bias.mood_bias[1] * 0.3f;
    finalMood.sad     += bias.mood_bias[2] * 0.2f;
    
    // Apply urgency boost to all negative moods
    finalMood.hungry  += bias.urgency_boost;
    finalMood.tired   += bias.urgency_boost * 0.8f;
    finalMood.sad     += bias.urgency_boost * 0.6f;
    
    // Apply small emotional noise to everything
    finalMood.happy      += bias.emotional_noise;
    finalMood.hungry     += bias.emotional_noise * 0.5f;
    finalMood.sad        += bias.emotional_noise;
    finalMood.neutral    += bias.emotional_noise * 0.3f;
    finalMood.concerned  += bias.emotional_noise * 0.4f;
    
    // Re-clamp all values after modification
    finalMood.happy      = constrain(finalMood.happy,      0.0f, 1.0f);
    finalMood.hungry     = constrain(finalMood.hungry,     0.0f, 1.0f);
    finalMood.sad        = constrain(finalMood.sad,        0.0f, 1.0f);
    finalMood.tired      = constrain(finalMood.tired,      0.0f, 1.0f);
    finalMood.dirty      = constrain(finalMood.dirty,      0.0f, 1.0f);
    finalMood.neutral    = constrain(finalMood.neutral,    0.0f, 1.0f);
    finalMood.excited    = constrain(finalMood.excited,    0.0f, 1.0f);
    finalMood.concerned  = constrain(finalMood.concerned,  0.0f, 1.0f);
    finalMood.spooked    = constrain(finalMood.spooked,    0.0f, 1.0f);
    
    return finalMood;
}

// ==============================================
// LAYER 4 → OUTPUT: RESOLVE FINAL MOOD
// ==============================================
DemiMood resolveFinalMood(const DemiMoodScores& scores) {
    // ✅ MOOD TRIGGERS AT EXACT STAT VALUES 20-30
    // Check raw stats directly - only trigger when stat is at 20 or 30
    extern int hunger, energy, cleanliness;
    
    if (hunger <= 20) return MOOD_HUNGRY;
    if (energy <= 20) return MOOD_TIRED;
    if (cleanliness <= 20) return MOOD_DIRTY;
    if (scores.spooked > 0.6f) return MOOD_SPOOKED;
    
    // Find highest scoring mood
    struct { float value; DemiMood mood; } moods[] = {
        { scores.tired,     MOOD_TIRED },    // ✅ TIRED NOW CHECKED FIRST
        { scores.hungry,    MOOD_HUNGRY },   // ✅ HUNGRY SECOND
        { scores.dirty,     MOOD_DIRTY },    // ✅ DIRTY THIRD
        { scores.excited,   MOOD_EXCITED },
        { scores.happy,     MOOD_HAPPY },
        { scores.happy - 0.15f, MOOD_CONTENT },
        { scores.concerned, MOOD_CONCERNED },
        { scores.sad,       MOOD_SAD },
        { scores.neutral,   MOOD_NEUTRAL }
    };
    
    float maxScore = -1.0f;
    DemiMood selected = MOOD_NEUTRAL;
    
    for (auto &m : moods) {
        if (m.value > maxScore) {
            maxScore = m.value;
            selected = m.mood;
        }
    }
    
    // ✅ Clamp happiness to never exceed 100
    // MOOD_MAX-1 is MOOD_HAPPY, ensuring happiness never over 100%
    return min(selected, (DemiMood)(MOOD_MAX - 1));
}

// System state
SystemState currentState = STATE_IDLE;

// Animation state
AnimationState animState = ANIM_IDLE;
unsigned long lastFrameTime = 0;
int currentFrame = 0;

// Number of frames per animation
const int IDLE_FRAMES = 2;
const int ALERT_FRAMES = 3;

// Mood string array
const char* moodStrings[] = {
    "Happy",      // MOOD_HAPPY
    "Content",   // MOOD_CONTENT
    "Neutral",   // MOOD_NEUTRAL
    "Concerned", // MOOD_CONCERNED
    "Sad",       // MOOD_SAD
    "Critical",  // MOOD_CRITICAL
    "Spooked",   // MOOD_SPOOKED
    "Excited",   // MOOD_EXCITED
    "Tired",     // MOOD_TIRED
    "Hungry",    // MOOD_HUNGRY
    "Dirty",     // MOOD_DIRTY
    "Sleeping"   // MOOD_SLEEPY
};

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

// Get mood string
const char* getMoodString(DemiMood mood) {
    return moodStrings[mood];
}

// Get Demi's current mood based on stats and AI state
DemiMood getDemiMood() {
    if (isSleeping) return MOOD_SLEEPY;

    // ==============================================
    // ✅ PROPER LAYERED FLOW
    // STATS → DRIVES → MOOD ENGINE → AI MODIFIER → OUTPUT
    // ==============================================
    
    // 1. GATHER BASE STATS (GROUND TRUTH)
    DemiStats stats = {
        hunger, happiness, energy, health, cleanliness, isSleeping
    };
    
    // 2. CALCULATE DRIVES (NEEDS)
    DemiDrives drives = calculateDrives(stats);
    
    // 3. CALCULATE BASE MOOD (RULE BASED - 100% DETERMINISTIC)
    DemiMoodScores baseMood = calculateBaseMood(drives);
    
    // 4. GET AI BIAS (ONLY SMALL MODIFIER)
    AIBias bias = moodModel.calculateBias(hunger, happiness, energy, health, cleanliness);
    ai.lastBias = bias;
    
    // 5. APPLY AI BIAS (MAX 30% INFLUENCE)
    DemiMoodScores finalMood = applyAIBias(baseMood, bias);
    
     // 6. RESOLVE FINAL MOOD
     DemiMood mood = resolveFinalMood(finalMood);
     
     // ✅ Ensure happiness never exceeds 100 (clamp final output)
     // This prevents any overflow from AI bias or mood engine
     mood = (DemiMood)min((int)mood, (int)(MOOD_MAX - 1));
    
    // WiFi "Spooked" Hard Override
    extern bool scaryNetworkFound;
    if (scaryNetworkFound) {
        mood = MOOD_SPOOKED;
    }
    
    // Apply personality bias from your interaction history
    if (ai.personality == PERS_NEGLECTFUL) {
        // Neglected Demi is more resilient - downgrade bad moods
        if (mood > MOOD_CONCERNED) mood = (DemiMood)max(0, mood - 1);
    }

    if (ai.personality == PERS_ATTENTIVE) {
        // Well cared for Demi is happier - upgrade good moods
        if (mood < MOOD_CONCERNED) mood = (DemiMood)min(11, mood - 1);
    }

    // Anticipation mood if waiting for care
    if (ai.prediction.anticipationMode && ai.prediction.confidence > 60) {
        if (mood == MOOD_NEUTRAL) mood = MOOD_CONTENT;
    }

    // Debug output (only if enabled via serial command)
    static unsigned long lastDebug = 0;
    extern bool aiDebugEnabled;
    if (moodModel.isReady() && aiDebugEnabled && millis() - lastDebug > 3000) {
        lastDebug = millis();
        Serial.printf("\n[SYSTEM] STATS: H=%d Hp=%d E=%d He=%d C=%d\n", hunger, happiness, energy, health, cleanliness);
        Serial.printf("[SYSTEM] DRIVES: Food=%.1f Rest=%.1f Clean=%.1f Attn=%.1f\n", 
            drives.needFood, drives.needRest, drives.needClean, drives.needAttention);
        Serial.printf("[AI] BIAS: Happy=%.2f Hungry=%.2f Sad=%.2f Urgency=%.2f Noise=%.2f\n",
            bias.mood_bias[0], bias.mood_bias[1], bias.mood_bias[2], bias.urgency_boost, bias.emotional_noise);
        Serial.printf("[FINAL] MOOD: %s\n", getMoodString(mood));
    }

    static int inferenceCounter = 0;
    if (inferenceCounter++ % 30 == 0) {
        neopixelWrite(RGB_LED_PIN, 0, 0, 5);
    }

    return mood;
}

// Called when user performs a care action (feed, pet, etc)
void onCareAction() {
    ai.consecutiveCares++;
    ai.totalCares++;
    ai.lastCareTime = millis();
    saveAll();
}

// Check if Demi has been neglected (critical stats with no care for 5 min)
void checkNeglect() {
    if ((hunger < 20 || happiness < 20) && 
        (millis() - ai.lastCareTime > 300000)) { // 5 min no care
        ai.neglectCount++;
        ai.consecutiveCares = 0;
    }
}

// Update Demi's stats over time
void updateStats() {
    static unsigned long lastUpdate = 0;
    unsigned long currentTime = millis();

    if (currentTime - lastUpdate >= STAT_UPDATE_INTERVAL_MS) { // Update every N seconds
        lastUpdate = currentTime;

        // Sleep mode: energy restores, hunger stays (or drops SLOWER), skip other drains
        if (isSleeping) {
            energy = min(100, energy + 2);  // Energy restores fast
            // Hunger stays same (metabolism slows, doesn't eat) - no gain no loss
            if (energy >= 100) {
                isSleeping = false;
                Serial.println("[Demi] Woke up from sleep!");
            }
            return;
        }

        // Deplete stats with ratios & differential timing
        static int tickCounter = 0;
        tickCounter++;
        
        // Base drain: slow for happiness, normal for others
        int hungerDrop = HUNGER_DROP_AMOUNT;
        int happinessDrop = HAPPINESS_DROP_AMOUNT;  // Drains every 4 ticks (20 sec) - slow baseline
        int energyDrop = ENERGY_DROP_AMOUNT;
        int cleanlinessDrop = CLEANLINESS_DROP_AMOUNT;
        
        // ✅ HAPPINESS DIRECTLY AFFECTED BY HUNGER AND ENERGY
        // Mood triggers at 20-30 range
        int happinessBonusDrain = 0;
        
        // Low hunger starts affecting happiness at 30, gets worse at 20
        if (hunger < 30) happinessBonusDrain += 1;
        if (hunger < 20) happinessBonusDrain += 2;
        
        // Low energy starts affecting happiness at 30, gets worse at 20
        if (energy < 30) happinessBonusDrain += 1;
        if (energy < 20) happinessBonusDrain += 2;
        
        // CRITICAL: When BOTH hunger and energy are in the 20-30 danger zone
        if (hunger < 30 && energy < 30) {
            happinessBonusDrain += 3;
        }
        
        if (cleanliness < 30) happinessDrop += 1;
        
        happinessDrop += happinessBonusDrain;
        
        // Energy drops faster when very happy (excited burns energy!)
        if (happiness > 70) energyDrop += 1;
        
        // Hunger drops faster when clean (demi notices hunger more)
        //if (cleanliness > 70) hungerDrop += 1;
        
        // Apply drains based on tick counter (differential timing)
        // Hunger: every tick (N seconds based on STAT_UPDATE_INTERVAL_MS)
        hunger = constrain(hunger - hungerDrop, 0, 100);
        
        // Energy: every N ticks
        if (tickCounter % ENERGY_DROP_INTERVAL == 0) {
            energy = constrain(energy - energyDrop, 0, 100);
        }
        if (tickCounter % HAPPINESS_DROP_INTERVAL == 0) {
            happiness = constrain(happiness - happinessDrop, 0, 100);
        }
        
        // Cleanliness: every N ticks
        if (tickCounter % CLEANLINESS_DROP_INTERVAL == 0) {
            cleanliness = constrain(cleanliness - cleanlinessDrop, 0, 100);
        }

        // ✅ HEALTH ONLY DROPS WHEN SURVIVAL STATS ARE CRITICAL
        // Health is stable until things get really bad
        
        if (hunger < 15 || energy < 15 || cleanliness < 15) {
            // Only start damaging health when any survival stat is <15%
            int damage = 0;
            if (hunger < 15)    damage += HEALTH_DROP_AMOUNT;
            if (energy < 15)    damage += HEALTH_DROP_AMOUNT * 2;
            if (cleanliness < 15) damage += HEALTH_DROP_AMOUNT;
            
            health = constrain(health - damage, 0, 100);
        }
        else if (hunger > 50 && energy > 50 && cleanliness > 50) {
            // Health is STABLE between 15% and 50% - no drain
            // Health RECOVERS slowly when all >50%
            if (tickCounter % 32 == 0) {
                health = min(100, health + 1);
            }
        }
        
        // Full fast recovery only when everything is good
        if (hunger > 70 && energy > 70 && cleanliness > 70 && happiness > 70) {
            if (tickCounter % 16 == 0) {
                health = min(100, health + 2);
            }
        }
        
        // Health slowly recovers when all stats are good (every 16 ticks = 80 sec)
        if (hunger > 60 && energy > 60 && cleanliness > 60 && happiness > 60) {
            if (tickCounter % 16 == 0) {
                health = min(100, health + 2);  // Slow recovery
            }
        }

        // Check for neglect
        checkNeglect();

        // Occasional humor (3% chance)
        if (random(100) < 3) {
            Serial.println("[Demi] *contemplating existence*");
        }
    }
}

// Save all stats and AI state to NVS
void saveAll() {
    Preferences prefs;
    prefs.begin("demi", false);
    prefs.putInt("hunger", hunger);
    prefs.putInt("happiness", happiness);
    prefs.putInt("energy", energy);
    prefs.putInt("health", health);
    prefs.putInt("cleanliness", cleanliness);
    prefs.putInt("isSleeping", isSleeping ? 1 : 0);
    prefs.putInt("cares", ai.consecutiveCares);      // shortened key
    prefs.putInt("neglect", ai.neglectCount);        // shortened key
    prefs.putInt("tcares", ai.totalCares);           // shortened key
    prefs.putInt("pers", ai.personality);
    prefs.putInt("attn", ai.attentionScore);
    prefs.putInt("rout", ai.routineScore);
    prefs.end();
}

// Load all stats and AI state from NVS
void loadAll() {
    Preferences prefs;
    prefs.begin("demi", true);
    hunger = constrain(prefs.getInt("hunger", 100), 0, 100);
    happiness = constrain(prefs.getInt("happiness", 100), 0, 100);
    energy = constrain(prefs.getInt("energy", 100), 0, 100);
    health = constrain(prefs.getInt("health", 100), 0, 100);
    cleanliness = constrain(prefs.getInt("cleanliness", 100), 0, 100);
    isSleeping = prefs.getInt("isSleeping", 0) == 1;
    ai.consecutiveCares = prefs.getInt("cares", 0);
    ai.neglectCount = prefs.getInt("neglect", 0);
    ai.totalCares = prefs.getInt("tcares", 0);
    ai.personality = (PersonalityType)prefs.getInt("pers", 0);
    ai.attentionScore = prefs.getInt("attn", 0);
    ai.routineScore = prefs.getInt("rout", 0);
    prefs.end();
    
    // Initialize TinyML mood model
    if (!moodModel.isReady()) {
        moodModel = DemiMoodModel();
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

    // BOTTOM: Mood display
    DemiMood mood = getDemiMood();
    u8g2.setCursor(0, 60);
    u8g2.print(getMoodString(mood));
    
    // Show AI status indicator
    if (moodModel.isReady()) {
        u8g2.setCursor(92, 60);
        u8g2.print("AI ✓");
    }

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
