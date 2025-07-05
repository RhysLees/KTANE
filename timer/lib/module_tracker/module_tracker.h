#pragma once

#include <Arduino.h>
#include <map>
#include "game_state_v2.h"

// Define an inclusive range for valid module CAN IDs
#define MODULE_ID_START 0x10
#define MODULE_ID_END   0x6F

struct ModuleInfo {
    unsigned long lastSeen;
};

class ModuleTracker {
private:
    std::map<uint8_t, ModuleInfo> modules;
    unsigned long timeoutMs = 5000; // Module timeout in ms
    GameStateManager* gameState;

public:
    ModuleTracker(GameStateManager* gsm);

    void handleCanMessage(uint8_t id, uint8_t* data, uint8_t len);
    void update();
    void reset();
};

void initModuleTracker(GameStateManager* gsm);
