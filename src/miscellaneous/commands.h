#ifndef COMMANDS_H
#define COMMANDS_H

#include <Arduino.h>
#include "esp_partition.h"
#include <SPIFFS.h>
//#include "freertos/FreeRTOS.h"
//#include "freertos/task.h"

// Demi stats externs for resetstats command
extern int hunger, happiness, energy, health, cleanliness;
extern bool isSleeping;
extern bool btnState_RB;
extern bool demiResetWaiting;
extern bool showResetProgress;
extern unsigned long demiResetStartTime;
extern bool demiResetReady;
extern bool aiDebugEnabled;
extern DemiAI ai;
void saveAll();

typedef void (*CommandFunction)(); 

struct Command {
    const char* name;        // Primary command (e.g., "help")
    const char* alias;       // Shortcut (e.g., "h") -> use "" for none
    const char* description; // Help text
    CommandFunction action;
};

// Action Functions
void showHelp(); 
void showAllTasks() {
    /*UBaseType_t uxArraySize = uxTaskGetNumberOfTasks();
    TaskStatus_t *pxTaskStatusArray = (TaskStatus_t *)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));

    if (pxTaskStatusArray != NULL) {
        // Generate raw status information about each task.
        uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);

        Serial.println("\n--- TASK LIST ---");
        Serial.printf("%-16s %-10s %-10s %-10s\n", "Name", "State", "Prio", "Stack HWM");
        Serial.println("--------------------------------------------------");

        for (UBaseType_t x = 0; x < uxArraySize; x++) {
            const char* state;
            switch (pxTaskStatusArray[x].eCurrentState) {
                case eRunning:   state = "Running";   break;
                case eReady:     state = "Ready";     break;
                case eBlocked:   state = "Blocked";   break;
                case eSuspended: state = "Suspended"; break;
                case eDeleted:   state = "Deleted";   break;
                default:         state = "Unknown";   break;
            }

            Serial.printf("%-16s %-10s %-10u %-10u\n",
                          pxTaskStatusArray[x].pcTaskName,
                          state,
                          (unsigned int)pxTaskStatusArray[x].uxCurrentPriority,
                          (unsigned int)pxTaskStatusArray[x].usStackHighWaterMark);
        }
        vPortFree(pxTaskStatusArray);
    } else {
        Serial.println("Memory allocation failed");
    }*/
    Serial.println("Task listing is currently disabled for stability reasons.");
}
inline void getHealth() { 
   
    // 1. RAM Calculation
    uint32_t totalInt = ESP.getHeapSize();
    uint32_t freeInt = ESP.getFreeHeap();
    uint32_t usedInt = totalInt - freeInt;

    // 2. PSRAM Calculation
    uint32_t totalPsram = ESP.getPsramSize();
    uint32_t freePsram = ESP.getFreePsram();
    uint32_t usedPsram = totalPsram - freePsram;

    Serial.println("\n--- LIVE MEMORY USAGE ---");

    Serial.println("[INTERNAL RAM]");
    Serial.printf("  USED: %6.2f KB | FREE: %6.2f KB | TOTAL: %.2f KB (%.1f%% Used)\n", 
        usedInt/1024.0, freeInt/1024.0, totalInt/1024.0, ((float)usedInt/totalInt)*100);

    Serial.println("\n[PSRAM (8MB R8)]");
    if (totalPsram > 0) {
        Serial.printf("  USED: %6.2f MB | FREE: %6.2f MB | TOTAL: %.2f MB (%.1f%% Used)\n", 
            usedPsram/(1024.0*1024.0), freePsram/(1024.0*1024.0), totalPsram/(1024.0*1024.0), ((float)usedPsram/totalPsram)*100);
    } else {
        Serial.println("  PSRAM not enabled!");
    }

    Serial.println("\n[FLASH PARTITION DETAILED USAGE (N16)]");
    esp_partition_iterator_t it = esp_partition_find(ESP_PARTITION_TYPE_ANY, ESP_PARTITION_SUBTYPE_ANY, NULL);
    while (it != NULL) {
        const esp_partition_t *p = esp_partition_get(it);
        float pSizeKB = p->size / 1024.0;
        float pSizeMB = p->size / (1024.0 * 1024.0);
        float pUsedKB = 0, pUsedMB = 0, pPerc = 0;

        // Logic to find "Used" based on room type
        if (String(p->label) == "app0") {
            pUsedKB = ESP.getSketchSize() / 1024.0;
            pUsedMB = pUsedKB / 1024.0;
            pPerc = (pUsedKB / pSizeKB) * 100;
        } 
        else if (String(p->label) == "spiffs") {
            if (SPIFFS.begin(true)) {
                pUsedKB = SPIFFS.usedBytes() / 1024.0;
                pUsedMB = pUsedKB / 1024.0;
                pPerc = (pUsedKB / pSizeKB) * 100;
            }
        }
        // app1, otadata, and nvs are system managed; showing as "Reserved/System"
        
        Serial.printf("  Room: %-10s | USED: %7.2f KB (%4.2f MB) | TOTAL: %7.2f KB (%4.2f MB) | %5.2f%%\n", 
            p->label, pUsedKB, pUsedMB, pSizeKB, pSizeMB, pPerc);
            
        it = esp_partition_next(it);
    }
    esp_partition_iterator_release(it);

    Serial.println("\n[SYSTEM SUMMARY]");
    Serial.printf("  Physical Chip Total: %.2f MB\n", ESP.getFlashChipSize()/(1024.0*1024.0));
    Serial.printf("  Internal Chip Temp:  %.2f °C\n", temperatureRead());
    Serial.printf("CPU FREQUENCY: %d MHz\n", getCpuFrequencyMhz());
    Serial.printf("SDK Version: %s\n", ESP.getSdkVersion());

    Serial.println("\n--- END OF HEALTH REPORT ---");

}

inline void playBeep()  { Serial.println("Buzzer triggered!"); }

// The Table
const Command commandTable[] = {
    // Name         Alias    Description             Function
    {"help",       "h",     "Show this help menu",  showHelp},
    {"health",     "hr",    "Show internal health", getHealth},
    {"beep",       "b",     "Trigger the buzzer",   playBeep},
    {"tasks",      "t",     "Show FreeRTOS tasks",  showAllTasks},
    {"ping",       "p",     "Ping test command",    [](){ Serial.println("Pong!"); }},
    {"reset",      "",      "Restart the device",  [](){ ESP.restart(); }},
    {"rst",        "",      "Start stats reset", [](){
        if (demiResetWaiting) {
            Serial.println("❌ Reset already in progress");
        } else {
            demiResetWaiting = true;
            demiResetStartTime = millis();
            Serial.println("TYPE 'DEMI' to confirm (type 'BACK' to cancel)");
        }
    }},
    {"DEMI", "", "Confirm reset", [](){
        if (!demiResetWaiting) {
            Serial.println("❌ Type 'rst' first to start reset");
        } else {
            Serial.println("✅ Hold RB for 5 seconds to reset...");
        }
    }},
    {"BACK", "", "Cancel reset", [](){
        if (demiResetWaiting) {
            demiResetWaiting = false;
            demiResetReady = false;
            showResetProgress = false;
            Serial.println("✅ Reset cancelled.");
        } else {
            Serial.println("No reset in progress.");
        }
    }},
    {"aidebug", "", "Toggle AI debug logs", [](){ 
        extern bool aiDebugEnabled;
        aiDebugEnabled = !aiDebugEnabled;
        Serial.printf("AI Debug logging: %s\n", aiDebugEnabled ? "ON" : "OFF");
    }},
};

const int cmdCount = sizeof(commandTable) / sizeof(Command);

inline void showHelp() {
    Serial.println("\n--- Available Commands ---");
    for (int i = 0; i < cmdCount; i++) {
        // If there is an alias, show it in brackets
        if (strlen(commandTable[i].alias) > 0) {
            Serial.printf("  %-7s [%s] : %s\n", commandTable[i].name, commandTable[i].alias, commandTable[i].description);
        } else {
            Serial.printf("  %-12s : %s\n", commandTable[i].name, commandTable[i].description);
        }
    }
    Serial.println("--------------------------\n");
}

#endif
