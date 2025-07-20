#pragma once

#include <Arduino.h>
#include <map>
#include "game_state_v2.h"

struct ModuleInfo {
    unsigned long lastHeartbeat;
    bool isRegistered;
    bool isActive;
    uint8_t lastStatus;
    uint8_t progress;
    bool isSolved;
    String moduleTypeName;
};

class ModuleTracker {
private:
    std::map<uint16_t, ModuleInfo> discoveredModules;  // All modules sending heartbeats
    std::map<uint16_t, ModuleInfo> registeredModules;  // Modules registered for game
    bool gameRunning;
    unsigned long lastDiscoveryReport;
    GameStateManager* gameState;
    
    // Timeout settings
    static const unsigned long DISCOVERY_TIMEOUT_MS = 3000;  // 3 seconds in discovery
    static const unsigned long GAME_TIMEOUT_MS = 10000;     // 10 seconds during game
    
    void processHeartbeat(uint16_t moduleId, const uint8_t* data, uint8_t len);
    void checkForTimeouts();
    void reportDiscoveredModules();
    String getModuleTypeName(uint16_t canId);

public:
    ModuleTracker(GameStateManager* gsm);
    
    void handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len);
    void update();
    void reset();
    
    // Game state management
    void setGameRunning(bool running);
    bool isGameRunning() const;
    
    // Registration management
    void registerModule(uint16_t moduleId);
    void unregisterModule(uint16_t moduleId);
    bool isModuleRegistered(uint16_t moduleId) const;
    
    // Status queries
    int getDiscoveredModuleCount() const;
    int getRegisteredModuleCount() const;
    std::map<uint16_t, ModuleInfo> getDiscoveredModules() const;
    std::map<uint16_t, ModuleInfo> getRegisteredModules() const;
    
    // Game error detection
    bool checkForCriticalErrors();
};

void initModuleTracker(GameStateManager* gsm);
ModuleTracker* getModuleTracker(); // Function to access the tracker instance
