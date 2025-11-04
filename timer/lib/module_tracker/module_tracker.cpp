#include "module_tracker.h"
#include <can_bus.h>

static ModuleTracker *trackerInstance = nullptr;

ModuleTracker::ModuleTracker(GameStateManager *gsm)
    : gameState(gsm), gameRunning(false), lastDiscoveryReport(0) {}

void ModuleTracker::handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len) {
    if (len < 1) return;
    
    // Data is clean (sender ID already removed), first byte is message type
    uint8_t msgType = data[0];
    
    // Process heartbeat messages (used for discovery when game is not running)
    if (msgType == MODULE_HEARTBEAT) {
        // Reduced logging to avoid blocking - only log periodically or for new modules
        // Serial.print("ModuleTracker: Received heartbeat from 0x");
        // Serial.print(senderId, HEX);
        // Serial.print(", gameRunning=");
        // Serial.println(gameRunning ? "true" : "false");
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
    if (moduleId == CAN_ID_TIMER) {
        // Serial.println("ModuleTracker: Ignoring heartbeat from timer itself");
        return;
    }
    
    unsigned long now = millis();
    
    // Parse heartbeat data: [MODULE_HEARTBEAT, status, solved_flag, progress]
    uint8_t status = (len >= 2) ? data[1] : 0;
    bool solved = (len >= 3) ? (data[2] != 0) : false;
    uint8_t progress = (len >= 4) ? data[3] : 0;
    
    // Check if this is a new module (first discovery via heartbeat)
    bool isNewModule = (discoveredModules.find(moduleId) == discoveredModules.end());
    
    // Update discovered modules (always track heartbeats)
    ModuleInfo& info = discoveredModules[moduleId];
    info.lastHeartbeat = now;
    info.isActive = true;
    info.lastStatus = status;
    info.progress = progress;
    info.isSolved = solved;
    info.moduleTypeName = getModuleTypeName(moduleId);
    
    // Only discover new modules when game is not running
    // Reduced logging to avoid blocking
    if (isNewModule) {
        // Serial.print("ModuleTracker: New module detected: 0x");
        // Serial.print(moduleId, HEX);
        // Serial.print(", gameRunning=");
        // Serial.print(gameRunning ? "true" : "false");
        // Serial.print(", gameState=");
        // Serial.println(gameState ? "valid" : "null");
    }
    
    if (isNewModule && !gameRunning && gameState) {
        // Register new module in game state
        uint8_t moduleType = (moduleId >> 5) & 0x7F;
        gameState->registerModule(moduleId, static_cast<ModuleType>(moduleType));
        
        // Also add to registered modules map
        registeredModules[moduleId] = info;
        registeredModules[moduleId].isRegistered = true;
        
        // Send discovery acknowledgment
        uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
        sendCanMessage(moduleId, discoveryAck, 1);
        // Removed delay - delays in callbacks can cause system hangs
        // CAN.sendMsgBuf() now handles buffer full gracefully
        
        // Only log once per new module to avoid flooding serial
        Serial.print("Module discovered: ");
        Serial.print(info.moduleTypeName);
        Serial.print(" (0x");
        Serial.print(moduleId, HEX);
        Serial.println(")");
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
            uint16_t moduleId = it->first;
            Serial.print("Module timeout: ");
            Serial.print(it->second.moduleTypeName);
            Serial.print(" (ID: 0x");
            Serial.print(moduleId, HEX);
            Serial.println(")");
            
            // If game is not running, remove from gamestate
            if (!gameRunning && gameState) {
                gameState->unregisterModule(moduleId);
            }
            
            it = discoveredModules.erase(it);
        } else {
            ++it;
        }
    }
    
    // Check registered modules (more critical during game)
    for (auto it = registeredModules.begin(); it != registeredModules.end();) {
        if (now - it->second.lastHeartbeat > timeoutMs) {
            uint16_t moduleId = it->first;
            
            if (gameRunning) {
                // If game is running, pause the game
                Serial.print("CRITICAL: Registered module timeout during game: ");
                Serial.print(it->second.moduleTypeName);
                Serial.print(" (ID: 0x");
                Serial.print(moduleId, HEX);
                Serial.println(") - pausing game");
                
                if (gameState && gameState->getState() == GameState::RUNNING) {
                    gameState->pauseTimer();
                }
            } else {
                // If game is not running, remove from gamestate
                Serial.print("Registered module timeout: ");
                Serial.print(it->second.moduleTypeName);
                Serial.print(" (ID: 0x");
                Serial.print(moduleId, HEX);
                Serial.println(") - removing from gamestate");
                
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
