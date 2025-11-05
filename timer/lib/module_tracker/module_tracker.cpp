#include "module_tracker.h"
#include <can_bus.h>

static ModuleTracker *trackerInstance = nullptr;

ModuleTracker::ModuleTracker(GameStateManager *gsm)
    : gameState(gsm), gameRunning(false), lastDiscoveryReport(0) {}

void ModuleTracker::handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len) {
    if (len < 1) return;
    
    uint8_t msgType = data[0];
    
    if (msgType == MODULE_HEARTBEAT) {
        processHeartbeat(senderId, data, len);
    }
    
    if (msgType == MODULE_REGISTER) {
        registerModule(senderId);
    }
}

void ModuleTracker::processHeartbeat(uint16_t moduleId, const uint8_t* data, uint8_t len) {
    if (moduleId == CAN_ID_TIMER) {
        return;
    }
    
    unsigned long now = millis();
    
    uint8_t status = (len >= 2) ? data[1] : 0;
    bool solved = (len >= 3) ? (data[2] != 0) : false;
    uint8_t progress = (len >= 4) ? data[3] : 0;
    
    bool isNewModule = (discoveredModules.find(moduleId) == discoveredModules.end());
    
    ModuleInfo& info = discoveredModules[moduleId];
    info.lastHeartbeat = now;
    info.isActive = true;
    info.lastStatus = status;
    info.progress = progress;
    info.isSolved = solved;
    info.moduleTypeName = getModuleTypeName(moduleId);
    
    if (isNewModule && !gameRunning && gameState) {
        uint8_t moduleType = (moduleId >> 5) & 0x7F;
        gameState->registerModule(moduleId, static_cast<ModuleType>(moduleType));
        registeredModules[moduleId] = info;
        registeredModules[moduleId].isRegistered = true;
        uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
        sendCanMessage(moduleId, discoveryAck, 1);
    }
    
    if (registeredModules.find(moduleId) != registeredModules.end()) {
        registeredModules[moduleId] = info;
        registeredModules[moduleId].isRegistered = true;
    }
}

void ModuleTracker::update() {
    checkForTimeouts();
    reportDiscoveredModules();
    
    if (gameRunning && checkForCriticalErrors()) {
        if (gameState) {
            gameState->setState(GameState::EXPLODED);
        }
    }
}

void ModuleTracker::checkForTimeouts() {
    unsigned long now = millis();
    unsigned long timeoutMs = gameRunning ? GAME_TIMEOUT_MS : DISCOVERY_TIMEOUT_MS;
    
    for (auto it = discoveredModules.begin(); it != discoveredModules.end();) {
        if (now - it->second.lastHeartbeat > timeoutMs) {
            uint16_t moduleId = it->first;
            if (!gameRunning && gameState) {
                gameState->unregisterModule(moduleId);
            }
            it = discoveredModules.erase(it);
        } else {
            ++it;
        }
    }
    
    for (auto it = registeredModules.begin(); it != registeredModules.end();) {
        if (now - it->second.lastHeartbeat > timeoutMs) {
            uint16_t moduleId = it->first;
            if (gameRunning) {
                if (gameState && gameState->getState() == GameState::RUNNING) {
                    gameState->pauseTimer();
                }
            } else {
                if (gameState) {
                    gameState->unregisterModule(moduleId);
                }
            }
            it = registeredModules.erase(it);
        } else {
            ++it;
        }
    }
}

void ModuleTracker::reportDiscoveredModules() {
}

String ModuleTracker::getModuleTypeName(uint16_t canId) {
    uint8_t moduleType = (canId >> 5) & 0x7F;
    uint8_t instanceId = canId & 0x1F;
    
    String name = String(getModuleTypeName(moduleType));
    if (instanceId > 0) {
        name += " #" + String(instanceId);
    }
    return name;
}

void ModuleTracker::setGameRunning(bool running) {
    gameRunning = running;
}

bool ModuleTracker::isGameRunning() const {
    return gameRunning;
}

void ModuleTracker::registerModule(uint16_t moduleId) {
    if (discoveredModules.find(moduleId) != discoveredModules.end()) {
        registeredModules[moduleId] = discoveredModules[moduleId];
        registeredModules[moduleId].isRegistered = true;
        
        // Notify game state manager
        if (gameState) {
            uint8_t moduleType = (moduleId >> 5) & 0x7F;
            gameState->registerModule(moduleId, static_cast<ModuleType>(moduleType));
        }
    }
}

void ModuleTracker::unregisterModule(uint16_t moduleId) {
    registeredModules.erase(moduleId);
    
    if (gameState) {
        gameState->unregisterModule(moduleId);
    }
}

bool ModuleTracker::isModuleRegistered(uint16_t moduleId) const {
    return registeredModules.find(moduleId) != registeredModules.end();
}

int ModuleTracker::getDiscoveredModuleCount() const {
    return discoveredModules.size();
}

int ModuleTracker::getRegisteredModuleCount() const {
    return registeredModules.size();
}

std::map<uint16_t, ModuleInfo> ModuleTracker::getDiscoveredModules() const {
    return discoveredModules;
}

std::map<uint16_t, ModuleInfo> ModuleTracker::getRegisteredModules() const {
    return registeredModules;
}

bool ModuleTracker::checkForCriticalErrors() {
    if (!gameRunning) return false;
    
    unsigned long now = millis();
    for (const auto& pair : registeredModules) {
        if (now - pair.second.lastHeartbeat > GAME_TIMEOUT_MS) {
            return true;
        }
    }
    return false;
}

void ModuleTracker::reset() {
    discoveredModules.clear();
    registeredModules.clear();
    gameRunning = false;
}

static void moduleTrackerCanCallback(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len) {
    if (trackerInstance) {
        trackerInstance->handleCanMessage(id, senderId, data, len);
    }
}

void initModuleTracker(GameStateManager *gsm) {
    static ModuleTracker tracker(gsm);
    trackerInstance = &tracker;
    registerCanCallback(moduleTrackerCanCallback);
}

ModuleTracker* getModuleTracker() {
    return trackerInstance;
}
