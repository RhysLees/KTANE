#include "game_state.h"
#include <can_bus.h>

bool Edgework::hasIndicator(IndicatorType type) const {
    for (const auto& ind : indicators) {
        if (ind.type == type) return true;
    }
    return false;
}

bool Edgework::hasLitIndicator(IndicatorType type) const {
    for (const auto& ind : indicators) {
        if (ind.type == type && ind.lit) return true;
    }
    return false;
}

bool Edgework::hasUnlitIndicator(IndicatorType type) const {
    for (const auto& ind : indicators) {
        if (ind.type == type && !ind.lit) return true;
    }
    return false;
}

bool Edgework::hasPort(PortType type) const {
    for (const auto& port : ports) {
        if (port.type == type) return true;
    }
    return false;
}

uint8_t Edgework::getLitIndicatorCount() const {
    uint8_t count = 0;
    for (const auto& ind : indicators) {
        if (ind.lit) count++;
    }
    return count;
}

uint8_t Edgework::getUnlitIndicatorCount() const {
    uint8_t count = 0;
    for (const auto& ind : indicators) {
        if (!ind.lit) count++;
    }
    return count;
}

uint8_t Edgework::getPortCount() const {
    return ports.size();
}

// ============================================================================
// AUDIO MODULE IMPLEMENTATION
// ============================================================================

void AudioModule::sendSound(uint8_t soundType) {
    uint8_t soundData[1] = {soundType};
    sendCanMessage(CAN_ID_AUDIO, soundData, 1);
}

void AudioModule::markSeen() {
    connected = true;
    lastSeen = millis();
}

void AudioModule::markDisconnected() {
    connected = false;
}

unsigned long AudioModule::getTimeSinceLastSeen() const {
    if (lastSeen == 0) return 0;
    return millis() - lastSeen;
}

// ============================================================================
// SERIAL MODULE IMPLEMENTATION
// ============================================================================

void SerialModule::setSerialNumber(const String& serial) {
    serialNumber = serial.substring(0, 6);
    sendSerialNumber();
}

void SerialModule::sendSerialNumber() {
    if (serialNumber.length() == 6) {
        uint8_t buf[7];
        buf[0] = SERIAL_DISPLAY_SET_SERIAL;
        memcpy(&buf[1], serialNumber.c_str(), 6);
        sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 7);
    }
}

void SerialModule::clear() {
    uint8_t buf[1] = {SERIAL_DISPLAY_CLEAR};
    sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 1);
}

void SerialModule::showCredit() {
    uint8_t buf[1] = {SERIAL_DISPLAY_SHOW_CREDIT};
    sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 1);
}

void SerialModule::markSeen() {
    connected = true;
    lastSeen = millis();
}

void SerialModule::markDisconnected() {
    connected = false;
}

unsigned long SerialModule::getTimeSinceLastSeen() const {
    if (lastSeen == 0) return 0;
    return millis() - lastSeen;
}

GameStateManager::GameStateManager() : config() {
}

GameStateManager::GameStateManager(const GameConfig& cfg) : config(cfg) {
}

void GameStateManager::initialize() {
    currentState = GameState::IDLE;
    stateChangeTime = millis();
    
    modules.clear();
    moduleMap.clear();
    
    // Reserve capacity upfront to prevent pointer invalidation on reallocation
    modules.reserve(32);  // Reserve for up to 32 modules
    
    generateSerialNumber();
    setupEdgework();
    
    Serial.println("Timer: System initialized and ready");
}

void GameStateManager::reset() {
    initialize();
}

void GameStateManager::tick() {
    updateTimer();
    updateNeedyModules();
    checkGameEndConditions();
    handleModuleTimeout();
    updateModuleConnections();  // Check for module disconnections
}

void GameStateManager::update() {
    tick();
}

void GameStateManager::setState(GameState newState) {
    if (currentState != newState) {
        GameState oldState = currentState;
        currentState = newState;
        stateChangeTime = millis();
        
        // Handle state-specific logic
        switch (newState) {
            case GameState::RUNNING:
                if (oldState == GameState::IDLE) {
                    gameStartTime = millis();
                }
                break;
            case GameState::EXPLODED:
                stopTimer();
                if (onStateChange) onStateChange(oldState, newState);
                break;
            case GameState::DEFUSED:
            case GameState::VICTORY:
                stopTimer();
                stats.wasVictory = true;
                if (onStateChange) onStateChange(oldState, newState);
                break;
            default:
                break;
        }
        
        if (onStateChange) onStateChange(oldState, newState);
    }
}

void GameStateManager::setTimeLimit(unsigned long ms) {
    timeLimitMs = ms;
    if (!timerRunning) {
        remainingMs = ms;
    }
}

void GameStateManager::startTimer() {
    if (!timerRunning) {
        lastUpdateTime = millis();
        timerRunning = true;
    }
}

void GameStateManager::stopTimer() {
    if (timerRunning) {
        updateTimer();
        timerRunning = false;
    }
}

void GameStateManager::pauseTimer() {
    if (timerRunning) {
        updateTimer();
        timerRunning = false;
        setState(GameState::PAUSED);
    }
}

void GameStateManager::resumeTimer() {
    if (!timerRunning && currentState == GameState::PAUSED) {
        lastUpdateTime = millis();
        timerRunning = true;
        setState(GameState::RUNNING);
    }
}

void GameStateManager::resetTimer() {
    remainingMs = timeLimitMs;
    lastUpdateTime = millis();
    timerRunning = false;
}

void GameStateManager::updateTimer() {
    if (!timerRunning) return;
    
    unsigned long now = millis();
    unsigned long delta = now - lastUpdateTime;
    lastUpdateTime = now;
    
    // Apply strike acceleration if enabled
    float adjustedDelta = delta;
    if (config.enableStrikeAcceleration) {
        float acceleration = 1.0f + (config.strikeAccelerationFactor * strikeCount);
        adjustedDelta = delta * acceleration;
    }
    
    if (adjustedDelta >= remainingMs) {
        remainingMs = 0;
        timerRunning = false;
        setState(GameState::EXPLODED);
    } else {
        remainingMs -= adjustedDelta;
    }
    
    if (onTimeUpdate) onTimeUpdate(remainingMs);
}

void GameStateManager::setStrikes(uint8_t strikes) {
    uint8_t oldStrikes = strikeCount;
    strikeCount = min(strikes, maxStrikes);
    
    if (strikeCount != oldStrikes) {
        uint8_t strikeData[2];
        strikeData[0] = TIMER_STRIKE_UPDATE;
        strikeData[1] = strikeCount;
        sendCanMessage(CAN_ID_BROADCAST, strikeData, 2);
        
        if (onStrikeChange) {
            onStrikeChange(strikeCount);
        }
    }
}

void GameStateManager::addStrike() {
    if (strikeCount < maxStrikes) {
        setStrikes(strikeCount + 1);
    }
}

void GameStateManager::clearStrikes() {
    setStrikes(0);
}

void GameStateManager::registerModule(uint16_t canId, ModuleType type) {
    if (moduleMap.find(canId) != moduleMap.end()) {
        return;
    }
    
    ModuleCategory category = getModuleCategory(type);
    
    // Reserve capacity to prevent reallocation (which would invalidate pointers in moduleMap)
    // Reserve for up to 32 modules to avoid frequent reallocations
    if (modules.capacity() < 32) {
        modules.reserve(32);
    }
    
    modules.emplace_back(canId, type, category);
    Module* newModule = &modules.back();
    moduleMap[canId] = newModule;
    
    if (category == ModuleCategory::NEEDY) {
        newModule->intervalMs = getNeedyModuleInterval(type);
        newModule->activationTime = millis() + newModule->intervalMs;
    }
}

unsigned long GameStateManager::getNeedyModuleInterval(ModuleType type) {
    switch (type) {
        case ModuleType::VENTING_GAS:
            return 30000; // 30 seconds
        case ModuleType::CAPACITOR_DISCHARGE:
            return 45000; // 45 seconds
        case ModuleType::KNOB:
            return 60000; // 60 seconds
        default:
            return 30000; // Default 30 seconds
    }
}

void GameStateManager::unregisterModule(uint16_t canId) {
    auto it = moduleMap.find(canId);
    if (it != moduleMap.end()) {
        // Remove from vector
        auto vecIt = std::find_if(modules.begin(), modules.end(),
            [canId](const Module& m) { return m.canId == canId; });
        if (vecIt != modules.end()) {
            modules.erase(vecIt);
        }
        moduleMap.erase(it);
    }
}

void GameStateManager::setModuleSolved(uint16_t canId) {
    Module* module = getModule(canId);
    if (module && !module->isSolved) {
        module->isSolved = true;
        module->isActive = false;
        
        uint8_t solved = getSolvedModules();
        uint8_t total = getTotalModules();
        
        if (onModuleSolved) onModuleSolved(solved, total);
        
        // Check if all regular modules are solved
        if (allModulesSolved()) {
            setState(GameState::DEFUSED);
        }
    }
}

void GameStateManager::setModuleActive(uint16_t canId, bool active) {
    Module* module = getModule(canId);
    if (module) {
        module->isActive = active;
        if (active) {
            module->activationTime = millis() + module->intervalMs;
        }
    }
}

void GameStateManager::updateModuleSeen(uint16_t canId) {
    Module* module = getModule(canId);
    if (module) {
        module->lastSeen = millis();
        module->isActive = true;  // Module is active if it's sending heartbeats
    }
}

bool GameStateManager::isModuleSolved(uint16_t canId) const {
    const Module* module = getModule(canId);
    return module ? module->isSolved : false;
}

bool GameStateManager::isModuleActive(uint16_t canId) const {
    const Module* module = getModule(canId);
    return module ? module->isActive : false;
}

Module* GameStateManager::getModule(uint16_t canId) {
    auto it = moduleMap.find(canId);
    return it != moduleMap.end() ? it->second : nullptr;
}

const Module* GameStateManager::getModule(uint16_t canId) const {
    auto it = moduleMap.find(canId);
    return it != moduleMap.end() ? it->second : nullptr;
}

uint8_t GameStateManager::getTotalModules() const {
    uint8_t count = 0;
    for (const auto& module : modules) {
        if (module.category == ModuleCategory::REGULAR) {
            count++;
        }
    }
    return count;
}

uint8_t GameStateManager::getSolvedModules() const {
    uint8_t count = 0;
    for (const auto& module : modules) {
        if (module.category == ModuleCategory::REGULAR && module.isSolved) {
            count++;
        }
    }
    return count;
}

uint8_t GameStateManager::getActiveModules() const {
    uint8_t count = 0;
    for (const auto& module : modules) {
        if (module.isActive) {
            count++;
        }
    }
    return count;
}

uint8_t GameStateManager::getNeedyModules() const {
    uint8_t count = 0;
    for (const auto& module : modules) {
        if (module.category == ModuleCategory::NEEDY) {
            count++;
        }
    }
    return count;
}

uint8_t GameStateManager::getRegularModules() const {
    return getTotalModules();
}

bool GameStateManager::allModulesSolved() const {
    uint8_t total = getTotalModules();
    uint8_t solved = getSolvedModules();
    return total > 0 && total == solved;
}

bool GameStateManager::hasActiveNeedyModules() const {
    return getActiveModules() > 0;
}

void GameStateManager::setSerialNumber(const String& serial) {
    serialModule.setSerialNumber(serial);
}

void GameStateManager::generateSerialNumber() {
    const char letters[] = "ABCDEFGHJKLMNPQRSTUVWXZ"; // A-Z excluding O and Y
    const char alphanum[] = "ABCDEFGHJKLMNPQRSTUVWXZ0123456789";
    const char digits[] = "0123456789";
    
    char serialBuf[7];
    serialBuf[0] = alphanum[random(sizeof(alphanum) - 1)];
    serialBuf[1] = alphanum[random(sizeof(alphanum) - 1)];
    serialBuf[2] = digits[random(10)];
    serialBuf[3] = letters[random(sizeof(letters) - 1)];
    serialBuf[4] = letters[random(sizeof(letters) - 1)];
    serialBuf[5] = alphanum[random(sizeof(alphanum) - 1)];
    serialBuf[6] = '\0';
    
    serialModule.setSerialNumber(String(serialBuf));
}

void GameStateManager::setupEdgework() {
    edgework.indicators.clear();
    edgework.ports.clear();
    edgework.batteryCount = 0;
    
    // Widget type constants
    const uint8_t WIDGET_BATTERY = 0;
    const uint8_t WIDGET_INDICATOR = 1;
    const uint8_t WIDGET_PORTS = 2;
    
    // Indicator labels and types
    const char* indicatorLabels[] = {"SND", "CLR", "CAR", "IND", "FRQ", "SIG", "NSA", "MSA", "TRN", "BOB", "FRK"};
    std::vector<IndicatorType> availableIndicatorTypes = {
        IndicatorType::SND, IndicatorType::CLR, IndicatorType::CAR, IndicatorType::IND,
        IndicatorType::FRQ, IndicatorType::SIG, IndicatorType::NSA, IndicatorType::MSA,
        IndicatorType::TRN, IndicatorType::BOB, IndicatorType::FRK
    };
    
    // Port labels
    const char* portLabels[] = {"PARALLEL", "SERIAL", "PS/2", "RJ-45", "RCA", "DVI-D", "STEREO-RCA"};
    
    // Generate exactly 5 widgets
    for (uint8_t widget = 0; widget < 5; widget++) {
        uint8_t widgetType = random(3); // 0=Battery, 1=Indicator, 2=Ports
        
        switch (widgetType) {
            case WIDGET_BATTERY: {
                // Coin flip between D Battery (1) or 2 AA Batteries (2)
                uint8_t batteryType = random(2); // 0 or 1
                if (batteryType == 0) {
                    edgework.batteryCount += 1; // D Battery
                } else {
                    edgework.batteryCount += 2; // 2 AA Batteries
                }
                break;
            }
            
            case WIDGET_INDICATOR: {
                // Only create indicator if we have available types (no repeats)
                if (!availableIndicatorTypes.empty()) {
                    // Select random indicator type from available ones
                    uint8_t typeIndex = random(availableIndicatorTypes.size());
                    IndicatorType type = availableIndicatorTypes[typeIndex];
                    
                    // Remove this type from available types (no repeats)
                    availableIndicatorTypes.erase(availableIndicatorTypes.begin() + typeIndex);
                    
                    // 60% chance lit (random float > 0.4)
                    bool lit = (random(1000) / 1000.0f) > 0.4f;
                    
                    String label = indicatorLabels[static_cast<uint8_t>(type)];
                    edgework.indicators.emplace_back(type, lit, label);
                }
                break;
            }
            
            case WIDGET_PORTS: {
                // Choose between Communication or I/O layout (coin flip)
                bool isCommLayout = random(2) == 0;
                
                if (isCommLayout) {
                    // Communication layout: Parallel, Serial
                    if (random(2) == 1) { // Coin flip for Parallel
                        edgework.ports.emplace_back(PortType::PARALLEL, "PARALLEL");
                    }
                    if (random(2) == 1) { // Coin flip for Serial
                        edgework.ports.emplace_back(PortType::SERIAL_PORT, "SERIAL");
                    }
                } else {
                    // I/O layout: DVI-D, PS/2, RJ-45, Stereo RCA
                    if (random(2) == 1) { // Coin flip for DVI-D
                        edgework.ports.emplace_back(PortType::DVI, "DVI-D");
                    }
                    if (random(2) == 1) { // Coin flip for PS/2
                        edgework.ports.emplace_back(PortType::PS2, "PS/2");
                    }
                    if (random(2) == 1) { // Coin flip for RJ-45
                        edgework.ports.emplace_back(PortType::RJ45, "RJ-45");
                    }
                    if (random(2) == 1) { // Coin flip for Stereo RCA
                        edgework.ports.emplace_back(PortType::STEREO_RCA, "STEREO-RCA");
                    }
                }
                break;
            }
        }
    }
}

bool GameStateManager::hasIndicator(IndicatorType type) const {
    return edgework.hasIndicator(type);
}

bool GameStateManager::hasLitIndicator(IndicatorType type) const {
    return edgework.hasLitIndicator(type);
}

bool GameStateManager::hasUnlitIndicator(IndicatorType type) const {
    return edgework.hasUnlitIndicator(type);
}

uint8_t GameStateManager::getLitIndicatorCount() const {
    return edgework.getLitIndicatorCount();
}

uint8_t GameStateManager::getUnlitIndicatorCount() const {
    return edgework.getUnlitIndicatorCount();
}

bool GameStateManager::hasPort(PortType type) const {
    return edgework.hasPort(type);
}

uint8_t GameStateManager::getPortCount() const {
    return edgework.getPortCount();
}

void GameStateManager::setConfig(const GameConfig& cfg) {
    config = cfg;
    maxStrikes = config.maxStrikes;
    timeLimitMs = config.timeLimitMs;
    if (!timerRunning) {
        remainingMs = timeLimitMs;
    }
}

void GameStateManager::setStateChangeCallback(std::function<void(GameState, GameState)> callback) {
    onStateChange = callback;
}

void GameStateManager::setStrikeChangeCallback(std::function<void(uint8_t)> callback) {
    onStrikeChange = callback;
}

void GameStateManager::setModuleSolvedCallback(std::function<void(uint8_t, uint8_t)> callback) {
    onModuleSolved = callback;
}

void GameStateManager::setTimeUpdateCallback(std::function<void(unsigned long)> callback) {
    onTimeUpdate = callback;
}

void GameStateManager::updateNeedyModules() {
    if (!config.enableNeedyModules) return;
    
    unsigned long now = millis();
    for (auto& module : modules) {
        if (module.category == ModuleCategory::NEEDY && !module.isActive) {
            if (now >= module.activationTime) {
                module.isActive = true;
                module.activationTime = now + module.intervalMs;
                
                // Send activation message to module
                uint8_t msg[] = {0x01}; // NEEDY_TRIGGER
                sendCanMessage(module.canId, msg, 1);
            }
        }
    }
}

void GameStateManager::checkGameEndConditions() {
    if (currentState != GameState::RUNNING) return;
    
    // Check for explosion conditions
    if (shouldExplode()) {
        setState(GameState::EXPLODED);
        return;
    }
    
    // Check for defusal conditions
    if (shouldDefuse()) {
        setState(GameState::DEFUSED);
        return;
    }
}

void GameStateManager::handleModuleTimeout() {
    unsigned long now = millis();
    unsigned long timeout = 5000; // 5 second timeout
    
    for (auto& module : modules) {
        if (module.lastSeen > 0 && (now - module.lastSeen) > timeout) {
            // Module hasn't been seen recently, mark as inactive
            module.isActive = false;
        }
    }
}

ModuleCategory GameStateManager::getModuleCategory(ModuleType type) const {
    switch (type) {
        case ModuleType::VENTING_GAS:
        case ModuleType::CAPACITOR_DISCHARGE:
        case ModuleType::KNOB:
            return ModuleCategory::NEEDY;
            
        case ModuleType::SERIAL_DISPLAY:
        case ModuleType::INDICATOR_PANEL:
        case ModuleType::BATTERY_HOLDER:
        case ModuleType::PORT_PANEL:
        case ModuleType::TIMER:
        case ModuleType::AUDIO:
            return ModuleCategory::IGNORED;
            
        default:
            return ModuleCategory::REGULAR;
    }
}

bool GameStateManager::isNeedyModule(ModuleType type) const {
    return getModuleCategory(type) == ModuleCategory::NEEDY;
}

bool GameStateManager::isIgnoredModule(ModuleType type) const {
    return getModuleCategory(type) == ModuleCategory::IGNORED;
}

// ============================================================================
// GAME LOGIC HELPERS
// ============================================================================

bool GameStateManager::shouldExplode() const {
    return remainingMs == 0 || strikeCount >= maxStrikes;
}

bool GameStateManager::shouldDefuse() const {
    return allModulesSolved() && !hasActiveNeedyModules();
}

float GameStateManager::getStrikeAcceleration() const {
    if (!config.enableStrikeAcceleration) return 1.0f;
    return 1.0f + (config.strikeAccelerationFactor * strikeCount);
}

bool GameStateManager::isEmergencyTime() const {
    return config.enableEmergencyAlarm && remainingMs < config.emergencyAlarmThreshold;
}

unsigned long GameStateManager::getTimeUntilExplosion() const {
    return remainingMs;
}

// ============================================================================
// STATISTICS
// ============================================================================

GameStateManager::GameStats GameStateManager::getStats() const {
    return stats;
}

void GameStateManager::resetStats() {
    stats = GameStats();
}

// ============================================================================
// CAN COMMUNICATION INTERFACE
// ============================================================================

void GameStateManager::handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    if (len < 1) {
        return;
    }
    
    // Data is clean (sender ID already removed), first byte is message type
    uint8_t msgType = data[0];
    
    switch (msgType) {
        case MODULE_REGISTER:
        {
            // Check if already registered
            bool alreadyRegistered = (moduleMap.find(senderId) != moduleMap.end());
            
            if (!alreadyRegistered) {
                uint8_t moduleType = (senderId >> 5) & 0x3F;
                registerModule(senderId, static_cast<ModuleType>(moduleType));
                uint8_t decodedModuleType, decodedInstanceId;
                decodeCanId(senderId, &decodedModuleType, &decodedInstanceId);
                Serial.print("Module registered: 0x");
                Serial.print(senderId, HEX);
                Serial.print(" (");
                Serial.print(getModuleTypeName(decodedModuleType));
                Serial.print(" #");
                Serial.print(decodedInstanceId);
                Serial.println(")");
                
                // Mark audio or serial module as seen if they're registering
                if (senderId == CAN_ID_AUDIO) {
                    audioModule.markSeen();
                } else if (senderId == CAN_ID_SERIAL_DISPLAY) {
                    serialModule.markSeen();
                }
                
                // Send module discovered acknowledgment
                uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
                sendCanMessage(senderId, discoveryAck, 1);
                
                // Send current state to newly registered module (only for new registrations)
                broadcastGameState(senderId);
            }
            // else - already registered, skip verbose logging and broadcast
            
            break;
        }
            
        case MODULE_STRIKE:
            {
                uint8_t moduleType, instanceId;
                decodeCanId(senderId, &moduleType, &instanceId);
                Serial.print("GameState: Strike received from module 0x");
                Serial.print(senderId, HEX);
                Serial.print(" (");
                Serial.print(getModuleTypeName(moduleType));
                Serial.print(" #");
                Serial.print(instanceId);
                Serial.println(")");
            }
            addStrike();
            // Strike callback will handle broadcasting
            break;
            
        case MODULE_SOLVED:
            {
                uint8_t moduleType, instanceId;
                decodeCanId(senderId, &moduleType, &instanceId);
                Serial.print("GameState: Module 0x");
                Serial.print(senderId, HEX);
                Serial.print(" (");
                Serial.print(getModuleTypeName(moduleType));
                Serial.print(" #");
                Serial.print(instanceId);
                Serial.println(") solved!");
            }
            setModuleSolved(senderId);
            // Module solved callback will handle broadcasting
            break;
            
        case MODULE_STATUS:
            // Update audio or serial module seen time
            if (senderId == CAN_ID_AUDIO) {
                audioModule.markSeen();
            } else if (senderId == CAN_ID_SERIAL_DISPLAY) {
                serialModule.markSeen();
            } else {
                updateModuleSeen(senderId);
            }
            
            // Handle additional status data if available
            if (len >= 5) {
                uint8_t moduleState = data[1];
                bool isSolved = (data[2] != 0);
                uint8_t progress = data[3];
                uint8_t strikes = data[4];
                
                // Update module solved status if changed
                Module* module = moduleMap[senderId];
                if (module && module->isSolved != isSolved) {
                    module->isSolved = isSolved;
                    if (isSolved) {
                        uint8_t moduleType, instanceId;
                        decodeCanId(senderId, &moduleType, &instanceId);
                        Serial.print("Status: Module 0x");
                        Serial.print(senderId, HEX);
                        Serial.print(" (");
                        Serial.print(getModuleTypeName(moduleType));
                        Serial.print(" #");
                        Serial.print(instanceId);
                        Serial.println(") solved via status update");
                    }
                }
            }
            break;
            
        case MODULE_HEARTBEAT:
            // Update last seen time for known modules
            if (senderId == CAN_ID_AUDIO) {
                audioModule.markSeen();
            } else if (senderId == CAN_ID_SERIAL_DISPLAY) {
                serialModule.markSeen();
            } else {
                // Auto-register module if not already registered (discovery via heartbeat)
                if (moduleMap.find(senderId) == moduleMap.end()) {
                    uint8_t moduleType = (senderId >> 5) & 0x3F;
                    registerModule(senderId, static_cast<ModuleType>(moduleType));
                    uint8_t decodedModuleType, decodedInstanceId;
                    decodeCanId(senderId, &decodedModuleType, &decodedInstanceId);
                    Serial.print("Module auto-registered via heartbeat: 0x");
                    Serial.print(senderId, HEX);
                    Serial.print(" (");
                    Serial.print(getModuleTypeName(decodedModuleType));
                    Serial.print(" #");
                    Serial.print(decodedInstanceId);
                    Serial.println(")");
                    
                    // Send module discovered acknowledgment
                    uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
                    sendCanMessage(senderId, discoveryAck, 1);
                    
                    // Send current state to newly registered module
                    broadcastGameState(senderId);
                }
                
                // Update last seen time
                updateModuleSeen(senderId);
                
                // Process enhanced heartbeat data if available
                if (len >= 4) {
                    uint8_t moduleState = data[1];
                    bool isSolved = (data[2] != 0);
                    uint8_t progress = data[3];
                    
                    // Update module solved status if changed
                    Module* module = moduleMap[senderId];
                    if (module && module->isSolved != isSolved) {
                        module->isSolved = isSolved;
                        if (isSolved) {
                            uint8_t moduleType, instanceId;
                            decodeCanId(senderId, &moduleType, &instanceId);
                            Serial.print("Module: Module 0x");
                            Serial.print(senderId, HEX);
                            Serial.print(" (");
                            Serial.print(getModuleTypeName(moduleType));
                            Serial.print(" #");
                            Serial.print(instanceId);
                            Serial.println(") solved via heartbeat");
                        }
                    }
                    
                    // Optional: Log detailed status for debugging
                    static unsigned long lastDetailedLog = 0;
                    if (millis() - lastDetailedLog > 30000) { // Every 30 seconds
                        uint8_t moduleType, instanceId;
                        decodeCanId(senderId, &moduleType, &instanceId);
                        Serial.print("Heartbeat: Module 0x");
                        Serial.print(senderId, HEX);
                        Serial.print(" (");
                        Serial.print(getModuleTypeName(moduleType));
                        Serial.print(" #");
                        Serial.print(instanceId);
                        Serial.print(") - State:");
                        Serial.print(moduleState);
                        Serial.print(" Solved:");
                        Serial.print(isSolved);
                        Serial.print(" Progress:");
                        Serial.println(progress);
                        lastDetailedLog = millis();
                    }
                }
            }
            break;
            
        // Handle epaper display messages
        case SERIAL_DISPLAY_CLEAR:
            {
                uint8_t moduleType, instanceId;
                decodeCanId(senderId, &moduleType, &instanceId);
                Serial.print("GameState: SERIAL_DISPLAY_CLEAR received from ID 0x");
                Serial.print(senderId, HEX);
                Serial.print(" (");
                Serial.print(getModuleTypeName(moduleType));
                Serial.print(" #");
                Serial.print(instanceId);
                Serial.println(") - epaper display ready");
            }
            if (senderId == CAN_ID_SERIAL_DISPLAY) {
                serialModule.markSeen();
            }
            break;
            
        default:
            // Reduced logging frequency for unknown messages to avoid blocking
            static unsigned long lastUnknownMsgLog = 0;
            static uint16_t unknownMsgCount = 0;
            unknownMsgCount++;
            if (millis() - lastUnknownMsgLog > 5000) { // Log summary every 5 seconds
                if (unknownMsgCount > 0) {
                    Serial.print("GameState: ");
                    Serial.print(unknownMsgCount);
                    Serial.println(" unknown message(s) received (suppressed detailed logs)");
                    unknownMsgCount = 0;
                }
                lastUnknownMsgLog = millis();
            }
            break;
    }
}

void GameStateManager::broadcastGameState(uint16_t targetId) {
    // Reduced logging to avoid blocking on Serial output
    // Removed delays - CAN.sendMsgBuf() now handles buffer full gracefully
    // Delays in callbacks can cause system hangs
    
    // Send serial number
    if (serialModule.getSerialNumber().length() == 6) {
        uint8_t serialData[7];
        serialData[0] = TIMER_SERIAL_NUMBER;
        memcpy(&serialData[1], serialModule.getSerialNumber().c_str(), 6);
        sendCanMessage(targetId, serialData, 7);
    }
    
    // Send strike count
    uint8_t strikeData[2];
    strikeData[0] = TIMER_STRIKE_UPDATE;
    strikeData[1] = strikeCount;
    sendCanMessage(targetId, strikeData, 2);
    
    // Send time remaining
    uint8_t timeData[5];
    timeData[0] = TIMER_TIME_UPDATE;
    uint32_t timeMs = remainingMs;
    memcpy(&timeData[1], &timeMs, 4);
    sendCanMessage(targetId, timeData, 5);
    
    // Send game state
    uint8_t gameStateData[1];
    if (currentState == GameState::RUNNING) {
        gameStateData[0] = TIMER_GAME_START;
    } else {
        gameStateData[0] = TIMER_GAME_STOP;
    }
    sendCanMessage(targetId, gameStateData, 1);
}

void GameStateManager::broadcastCountdown(uint8_t seconds) {
    uint8_t countdownData[2];
    countdownData[0] = TIMER_COUNTDOWN;
    countdownData[1] = seconds;
    sendCanMessage(CAN_ID_BROADCAST, countdownData, 2);
}





void GameStateManager::createNewGame() {
    Serial.println("GameState: Creating new game...");
    
    // Initialize game state
    gameStartTime = millis();
    
    timeLimitMs = config.timeLimitMs;
    remainingMs = timeLimitMs;
    lastUpdateTime = millis();
    timerRunning = false;
    
    strikeCount = 0;
    maxStrikes = config.maxStrikes;
    
    resetStats();
    
    // Set to IDLE state (ready to start)
    setState(GameState::IDLE);
    
    Serial.println("GameState: New game created - ready to start!");
    Serial.print("GameState: Found ");
    Serial.print(getTotalModules());
    Serial.println(" regular modules");
    
    // Send serial number to epaper display
    serialModule.sendSerialNumber();
    Serial.print("GameState: Sent serial number to display: ");
    Serial.println(serialModule.getSerialNumber());
}

void GameStateManager::startGame() {
    if (currentState != GameState::IDLE) {
        Serial.println("GameState: Cannot start game - not in IDLE state");
        return;
    }
    
    Serial.println("GameState: Starting game...");
    
    // Broadcast to all modules via broadcast ID
    uint8_t gameStartData[1] = {TIMER_GAME_START};
    sendCanMessage(CAN_ID_BROADCAST, gameStartData, 1);
    
    // Then start the timer
    setState(GameState::RUNNING);
    startTimer();
    
    Serial.println("GameState: Game started successfully!");
}

// ============================================================================
// AUDIO MODULE MANAGEMENT
// ============================================================================

void GameStateManager::updateAudioModuleSeen() {
    audioModule.markSeen();
}

void GameStateManager::sendAudioSound(uint8_t soundType) {
    audioModule.sendSound(soundType);
}

// ============================================================================
// SERIAL MODULE MANAGEMENT
// ============================================================================

void GameStateManager::updateSerialModuleSeen() {
    serialModule.markSeen();
}

// ============================================================================
// MODULE CONNECTION TRACKING
// ============================================================================

void GameStateManager::updateModuleConnections() {
    unsigned long now = millis();
    const unsigned long MODULE_TIMEOUT_MS = 5000;  // 5 second timeout
    
    // Check audio module
    if (audioModule.isConnected() && 
        audioModule.getTimeSinceLastSeen() > MODULE_TIMEOUT_MS) {
        audioModule.markDisconnected();
        Serial.println("Audio module disconnected");
    }
    
    // Check serial module
    if (serialModule.isConnected() && 
        serialModule.getTimeSinceLastSeen() > MODULE_TIMEOUT_MS) {
        serialModule.markDisconnected();
        Serial.println("Serial module disconnected");
    }
    
    // Check all registered modules
    for (auto& module : modules) {
        if (module.lastSeen > 0 && (now - module.lastSeen) > MODULE_TIMEOUT_MS) {
            // Module hasn't been seen recently, mark as inactive
            module.isActive = false;
        }
    }
}

bool GameStateManager::isModuleConnected(uint16_t canId) const {
    // Check audio module
    if (canId == CAN_ID_AUDIO) {
        return audioModule.isConnected();
    }
    
    // Check serial module
    if (canId == CAN_ID_SERIAL_DISPLAY) {
        return serialModule.isConnected();
    }
    
    // Check registered modules
    auto it = moduleMap.find(canId);
    if (it != moduleMap.end()) {
        const Module* module = it->second;
        if (module && module->lastSeen > 0) {
            unsigned long timeSince = millis() - module->lastSeen;
            return timeSince <= 5000;  // 5 second timeout
        }
    }
    
    return false;
}

unsigned long GameStateManager::getModuleLastSeen(uint16_t canId) const {
    // Check audio module
    if (canId == CAN_ID_AUDIO) {
        return audioModule.getTimeSinceLastSeen();
    }
    
    // Check serial module
    if (canId == CAN_ID_SERIAL_DISPLAY) {
        return serialModule.getTimeSinceLastSeen();
    }
    
    // Check registered modules
    auto it = moduleMap.find(canId);
    if (it != moduleMap.end()) {
        const Module* module = it->second;
        if (module && module->lastSeen > 0) {
            return millis() - module->lastSeen;
        }
    }
    
    return 0;
}

void GameStateManager::clearSerialDisplay() {
    serialModule.clear();
}

void GameStateManager::showSerialCredit() {
    serialModule.showCredit();
}