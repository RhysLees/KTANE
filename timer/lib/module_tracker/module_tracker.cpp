#include "module_tracker.h"
#include <can_bus.h>

static ModuleTracker *trackerInstance = nullptr;

ModuleTracker::ModuleTracker(GameStateManager *gsm)
    : gameState(gsm), gameRunning(false), lastDiscoveryReport(0) {}

void ModuleTracker::handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len) {
    if (len < 1) return;
    
    // Data is clean (sender ID already removed), first byte is message type
    uint8_t msgType = data[0];
    
    // Only process heartbeat messages
    if (msgType == MODULE_HEARTBEAT) {
        processHeartbeat(senderId, data, len);
    }
    
    // Handle registration messages
    if (msgType == MODULE_REGISTER) {
        registerModule(senderId);
        Serial.print("Module registered: ");
        Serial.print(getModuleTypeName(senderId));
        Serial.print(" (ID: 0x");
        Serial.print(senderId, HEX);
        Serial.println(")");
    }
}

void ModuleTracker::processHeartbeat(uint16_t moduleId, const uint8_t* data, uint8_t len) {
    // Skip timer module's own messages
    if (moduleId == CAN_ID_TIMER) return;
    
    unsigned long now = millis();
    
    // Parse heartbeat data: [MODULE_HEARTBEAT, status, solved_flag, progress]
    uint8_t status = (len >= 2) ? data[1] : 0;
    bool solved = (len >= 3) ? (data[2] != 0) : false;
    uint8_t progress = (len >= 4) ? data[3] : 0;
    
    // Check if this is a new module (first discovery)
    bool isNewModule = (discoveredModules.find(moduleId) == discoveredModules.end());
    
    // Update discovered modules (always track heartbeats)
    ModuleInfo& info = discoveredModules[moduleId];
    info.lastHeartbeat = now;
    info.isActive = true;
    info.lastStatus = status;
    info.progress = progress;
    info.isSolved = solved;
    info.moduleTypeName = getModuleTypeName(moduleId);
    
    // Send discovery acknowledgment if this is a new module
    if (isNewModule) {
        uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
        sendCanMessage(moduleId, discoveryAck, 1);
        Serial.print("Module discovered: ");
        Serial.print(info.moduleTypeName);
        Serial.print(" (ID: 0x");
        Serial.print(moduleId, HEX);
        Serial.println(") - sent discovery acknowledgment");
    }
    
    // If module is registered for game, update registered modules too
    if (registeredModules.find(moduleId) != registeredModules.end()) {
        registeredModules[moduleId] = info;
        registeredModules[moduleId].isRegistered = true;
    }
}

void ModuleTracker::update() {
    checkForTimeouts();
    reportDiscoveredModules();
    
    // Check for critical errors during game
    if (gameRunning && checkForCriticalErrors()) {
        Serial.println("CRITICAL: Registered module lost during game - stopping game!");
        if (gameState) {
            gameState->setState(GameState::EXPLODED); // Set game to exploded state
        }
    }
}

void ModuleTracker::checkForTimeouts() {
    unsigned long now = millis();
    unsigned long timeoutMs = gameRunning ? GAME_TIMEOUT_MS : DISCOVERY_TIMEOUT_MS;
    
    // Check discovered modules
    for (auto it = discoveredModules.begin(); it != discoveredModules.end();) {
        if (now - it->second.lastHeartbeat > timeoutMs) {
            Serial.print("Module timeout: ");
            Serial.print(it->second.moduleTypeName);
            Serial.print(" (ID: 0x");
            Serial.print(it->first, HEX);
            Serial.println(")");
            it = discoveredModules.erase(it);
        } else {
            ++it;
        }
    }
    
    // Check registered modules (more critical during game)
    for (auto it = registeredModules.begin(); it != registeredModules.end();) {
        if (now - it->second.lastHeartbeat > timeoutMs) {
            Serial.print("CRITICAL: Registered module timeout: ");
            Serial.print(it->second.moduleTypeName);
            Serial.print(" (ID: 0x");
            Serial.print(it->first, HEX);
            Serial.println(")");
            it = registeredModules.erase(it);
        } else {
            ++it;
        }
    }
}

void ModuleTracker::reportDiscoveredModules() {
    unsigned long now = millis();
    if (now - lastDiscoveryReport >= 5000) { // Report every 5 seconds
        lastDiscoveryReport = now;
        
        if (!gameRunning && discoveredModules.size() > 0) {
            Serial.print("Discovered modules (");
            Serial.print(discoveredModules.size());
            Serial.print("): ");
            
            bool first = true;
            for (const auto& pair : discoveredModules) {
                if (!first) Serial.print(", ");
                Serial.print(pair.second.moduleTypeName);
                first = false;
            }
            Serial.println();
        }
    }
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
    if (gameRunning != running) {
        gameRunning = running;
        Serial.print("Module Tracker: Game state changed to ");
        Serial.println(running ? "RUNNING" : "DISCOVERY");
        
        if (running) {
            Serial.print("Registered modules for game: ");
            Serial.println(registeredModules.size());
        }
    }
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
    } else {
        Serial.print("Warning: Attempted to register unknown module: 0x");
        Serial.println(moduleId, HEX);
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
    
    // Check if any registered modules have timed out
    unsigned long now = millis();
    for (const auto& pair : registeredModules) {
        if (now - pair.second.lastHeartbeat > GAME_TIMEOUT_MS) {
            Serial.print("CRITICAL ERROR: Module ");
            Serial.print(pair.second.moduleTypeName);
            Serial.println(" stopped responding during game!");
            return true;
        }
    }
    return false;
}

void ModuleTracker::reset() {
    discoveredModules.clear();
    registeredModules.clear();
    gameRunning = false;
    Serial.println("Module tracker reset");
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
    Serial.println("Module tracker initialized");
}

ModuleTracker* getModuleTracker() {
    return trackerInstance;
}
