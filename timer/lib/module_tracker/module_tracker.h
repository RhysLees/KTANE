#pragma once

#include <Arduino.h>
#include <map>
#include "game_state_v2.h"

// Define an inclusive range for valid module CAN IDs
// Real CAN IDs are 16-bit constructed as: (moduleType << 5) | instanceId
#define MODULE_ID_START 0x200  // Start of actual module IDs
#define MODULE_ID_END   0x6FF  // End of actual module IDs

struct ModuleInfo {
    unsigned long lastSeen;
};

class ModuleTracker {
private:
    std::map<uint16_t, ModuleInfo> modules;
    unsigned long timeoutMs = 5000; // Module timeout in ms
    GameStateManager* gameState;

public:
    ModuleTracker(GameStateManager* gsm);

    void handleCanMessage(uint16_t id, uint8_t* data, uint8_t len);
    void update();
    void reset();
};

void initModuleTracker(GameStateManager* gsm);
