#ifndef FOOD_ITEM_H
#define FOOD_ITEM_H

#include <Arduino.h>

class FoodItem {
private:
    const char* name;
    uint8_t hungerRestore;
    uint8_t energyBoost;
    uint8_t happinessBoost;
    FoodItem(const char* n, uint8_t hunger, uint8_t energy, uint8_t happy) {
        name = n;
        hungerRestore = hunger;
        energyBoost = energy;
        happinessBoost = happy;
    }

    const char* getName() { return name; }
    uint8_t getHungerRestore() { return hungerRestore; }
    uint8_t getEnergyBoost() { return energyBoost; }
    uint8_t getHappinessBoost() { return happinessBoost; }

    void apply() {
        // Will apply stats to Demi later
    }
};

#endif
