#include "game_state_v2.h"
#include <can_bus.h>

// ============================================================================
// EDGEWORK HELPER METHODS
// ============================================================================

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
// GAME STATE MANAGER IMPLEMENTATION
// ============================================================================

GameStateManager::GameStateManager() : config() {
    // Don't call initialize() here - call it manually in setup()
}

GameStateManager::GameStateManager(const GameConfig& cfg) : config(cfg) {
    // Don't call initialize() here - call it manually in setup()
}

void GameStateManager::initialize() {
    // Start in discovery mode - don't initialize game state yet
    currentState = GameState::DISCOVERY;
    stateChangeTime = millis();
    
    // Clear any existing modules
    modules.clear();
    moduleMap.clear();
    
    // Generate serial number and edgework for display during discovery
    generateSerialNumber();
    setupEdgework();
    
    // Enter discovery mode
    enterDiscoveryMode();
    
    Serial.println("Timer: Starting in Discovery Mode");
    Serial.println("Timer: Power on modules and press rotary wheel when ready");
}

void GameStateManager::reset() {
    initialize();
}

void GameStateManager::tick() {
    // Handle discovery mode if active
    updateDiscoveryMode();
    
    // Only run game logic if not in discovery mode
    if (!isInDiscoveryMode()) {
        updateTimer();
        updateNeedyModules();
        checkGameEndConditions();
        handleModuleTimeout();
    }
}

void GameStateManager::update() {
    tick();
}

// ============================================================================
// STATE MANAGEMENT
// ============================================================================

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

// ============================================================================
// TIMER MANAGEMENT
// ============================================================================

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

// ============================================================================
// STRIKE MANAGEMENT
// ========================================================================

void GameStateManager::setStrikes(uint8_t strikes) {
    uint8_t oldStrikes = strikeCount;
    strikeCount = min(strikes, maxStrikes);
    
    if (strikeCount != oldStrikes) {
        // Broadcast strike update to all modules
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

// ============================================================================
// MODULE MANAGEMENT
// ============================================================================

void GameStateManager::registerModule(uint16_t canId, ModuleType type) {
    // Check if module already exists
    if (moduleMap.find(canId) != moduleMap.end()) {
        return;
    }
    
    ModuleCategory category = getModuleCategory(type);
    modules.emplace_back(canId, type, category);
    moduleMap[canId] = &modules.back();
    
    // Track last module registration time for adaptive discovery
    last_module_registration_time = millis();
    
    // Set activation time for needy modules
    if (category == ModuleCategory::NEEDY) {
        Module* module = moduleMap[canId];
        module->intervalMs = getNeedyModuleInterval(type);
        module->activationTime = millis() + module->intervalMs;
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

// ============================================================================
// SERIAL NUMBER
// ============================================================================

void GameStateManager::setSerialNumber(const String& serial) {
    serialNumber = serial.substring(0, 6);
    
    // Send to serial display module
    uint8_t buf[7];
    buf[0] = SERIAL_DISPLAY_SET_SERIAL;
    memcpy(&buf[1], serialNumber.c_str(), 6);
    sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 7);
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
    
    setSerialNumber(String(serialBuf));
}

// ============================================================================
// EDGEWORK MANAGEMENT
// ============================================================================

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

// ============================================================================
// CONFIGURATION
// ============================================================================

void GameStateManager::setConfig(const GameConfig& cfg) {
    config = cfg;
    maxStrikes = config.maxStrikes;
    timeLimitMs = config.timeLimitMs;
    if (!timerRunning) {
        remainingMs = timeLimitMs;
    }
}

// ============================================================================
// CALLBACKS
// ============================================================================

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

// ============================================================================
// INTERNAL METHODS
// ============================================================================

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
// DEBUG & UTILITY
// ============================================================================

// Debug functions removed

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

void GameStateManager::handleCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // New standardized message format: [senderType, senderInstance, messageType, ...messageData]
    if (len < 3) {
        return;
    }
    
    uint8_t senderType = data[0];
    uint8_t senderInstance = data[1];
    uint8_t msgType = data[2];
    uint16_t senderCanId = CAN_INSTANCE_ID(senderType, senderInstance);
    
    if (len >= 3) {
        
        switch (msgType) {
            case MODULE_REGISTER:
            {
                // Registration info is now in sender fields
                // Check if already registered (from heartbeat)
                bool alreadyRegistered = (moduleMap.find(senderCanId) != moduleMap.end());
                
                if (!alreadyRegistered) {
                    registerModule(senderCanId, static_cast<ModuleType>(senderType));
                    Serial.print("Discovery: Explicit registration - ID: 0x");
                    Serial.print(senderCanId, HEX);
                    Serial.print(", Type: 0x");
                    Serial.println(senderType, HEX);
                    
                    // In discovery mode, this counts as a new registration
                    if (isInDiscoveryMode()) {
                        last_module_registration_time = millis();
                    }
                } else {
                    Serial.print("Discovery: Module 0x");
                    Serial.print(senderCanId, HEX);
                    Serial.println(" already registered via heartbeat");
                }
                
                // Send current state to newly registered module (if not in discovery mode)
                if (!isInDiscoveryMode()) {
                    broadcastGameState(senderCanId);
                }
                break;
            }
                
            case MODULE_STRIKE:
                Serial.print("GameState: Strike received from module 0x");
                Serial.println(senderCanId, HEX);
                addStrike();
                // Strike callback will handle broadcasting
                break;
                
            case MODULE_SOLVED:
                Serial.print("GameState: Module 0x");
                Serial.print(senderCanId, HEX);
                Serial.println(" solved!");
                setModuleSolved(senderCanId);
                // Module solved callback will handle broadcasting
                break;
                
            case MODULE_STATUS:
                updateModuleSeen(senderCanId);
                
                // Handle additional status data if available (starts at index 3)
                if (len >= 7) {
                    uint8_t moduleState = data[3];
                    bool isSolved = (data[4] != 0);
                    uint8_t progress = data[5];
                    uint8_t strikes = data[6];
                    
                    // Update module solved status if changed
                    Module* module = moduleMap[senderCanId];
                    if (module && module->isSolved != isSolved) {
                        module->isSolved = isSolved;
                        if (isSolved) {
                            Serial.print("Status: Module 0x");
                            Serial.print(senderCanId, HEX);
                            Serial.println(" solved via status update");
                        }
                    }
                }
                break;
                
            case MODULE_HEARTBEAT:
                // Auto-register unknown modules from heartbeats
                if (moduleMap.find(senderCanId) == moduleMap.end()) {
                    Serial.print("Discovery: Auto-registering module from heartbeat - ID: 0x");
                    Serial.print(senderCanId, HEX);
                    Serial.print(", Type: 0x");
                    Serial.println(senderType, HEX);
                    
                    registerModule(senderCanId, static_cast<ModuleType>(senderType));
                    
                    // In discovery mode, this counts as a new registration
                    if (isInDiscoveryMode()) {
                        last_module_registration_time = millis();
                    }
                } else {
                    // Known module - update last seen time
                    updateModuleSeen(senderCanId);
                }
                
                // Process enhanced heartbeat data if available (starts at index 3)
                if (len >= 6) {
                    uint8_t moduleState = data[3];
                    bool isSolved = (data[4] != 0);
                    uint8_t progress = data[5];
                    
                    // Update module solved status if changed
                    Module* module = moduleMap[senderCanId];
                    if (module && module->isSolved != isSolved) {
                        module->isSolved = isSolved;
                        if (isSolved) {
                            Serial.print("Discovery: Module 0x");
                            Serial.print(senderCanId, HEX);
                            Serial.println(" solved via heartbeat");
                        }
                    }
                    
                    // Optional: Log detailed status for debugging
                    static unsigned long lastDetailedLog = 0;
                    if (millis() - lastDetailedLog > 30000) { // Every 30 seconds
                        Serial.print("Heartbeat: Module 0x");
                        Serial.print(senderCanId, HEX);
                        Serial.print(" - State:");
                        Serial.print(moduleState);
                        Serial.print(" Solved:");
                        Serial.print(isSolved);
                        Serial.print(" Progress:");
                        Serial.println(progress);
                        lastDetailedLog = millis();
                    }
                }
                break;
                
            // Handle epaper display messages
            case SERIAL_DISPLAY_CLEAR:
                Serial.print("GameState: SERIAL_DISPLAY_CLEAR received from ID 0x");
                Serial.print(id, HEX);
                Serial.println(" - epaper display ready");
                break;
                
            default:
                Serial.print("GameState: Unknown message type 0x");
                Serial.print(msgType, HEX);
                Serial.print(" from module 0x");
                Serial.println(id, HEX);
                break;
        }
    }
}

void GameStateManager::broadcastGameState(uint16_t targetId) {
    Serial.print("GameState: Broadcasting game state to ID 0x");
    Serial.println(targetId, HEX);
    
    // Send serial number
    uint8_t serialData[7];
    serialData[0] = TIMER_SERIAL_NUMBER;
    memcpy(&serialData[1], serialNumber.c_str(), 6);
    sendCanMessage(targetId, serialData, 7);
    Serial.print("GameState: Sent serial number: ");
    Serial.println(serialNumber);
    
    // Send strike count
    uint8_t strikeData[2];
    strikeData[0] = TIMER_STRIKE_UPDATE;
    strikeData[1] = strikeCount;
    sendCanMessage(targetId, strikeData, 2);
    Serial.print("GameState: Sent strike count: ");
    Serial.println(strikeCount);
    
    // Send time remaining
    uint8_t timeData[5];
    timeData[0] = TIMER_TIME_UPDATE;
    uint32_t timeMs = remainingMs;
    memcpy(&timeData[1], &timeMs, 4);
    sendCanMessage(targetId, timeData, 5);
    Serial.print("GameState: Sent time remaining: ");
    Serial.println(timeMs / 1000);
    
    // Send game state
    uint8_t gameStateData[1];
    if (currentState == GameState::RUNNING) {
        gameStateData[0] = TIMER_GAME_START;
        Serial.println("GameState: Sent TIMER_GAME_START");
    } else {
        gameStateData[0] = TIMER_GAME_STOP;
        Serial.println("GameState: Sent TIMER_GAME_STOP");
    }
    sendCanMessage(targetId, gameStateData, 1);
    
    Serial.println("GameState: Game state broadcast complete");
}

void GameStateManager::broadcastCountdown(uint8_t seconds) {
    uint8_t countdownData[2];
    countdownData[0] = TIMER_COUNTDOWN;
    countdownData[1] = seconds;
    Serial.print("DEBUG: About to send countdown broadcast: ");
    Serial.println(seconds);
    sendCanMessage(CAN_ID_BROADCAST, countdownData, 2);
    
    Serial.print("GameState: Countdown - ");
    Serial.print(seconds);
    Serial.println(" seconds");
}



// ============================================================================
// DISCOVERY MODE IMPLEMENTATION
// ============================================================================

void GameStateManager::enterDiscoveryMode() {
    discoveryMode = true;
    discovery_start_time = millis();
    last_module_registration_time = 0;
    setState(GameState::DISCOVERY);
    
    Serial.println("Discovery: Entered discovery mode - actively scanning for modules");
    Serial.print("Discovery: Current modules registered: ");
    Serial.println(modules.size());
}

void GameStateManager::exitDiscoveryMode() {
    discoveryMode = false;
    
    Serial.println("Discovery: Exiting discovery mode");
    Serial.print("Discovery: Final module count: ");
    Serial.println(modules.size());
    
    // Create new game with discovered modules
    createNewGame();
}

bool GameStateManager::isInDiscoveryMode() const {
    return discoveryMode && currentState == GameState::DISCOVERY;
}

void GameStateManager::updateDiscoveryMode() {
    if (!isInDiscoveryMode()) return;
    
    // Discovery mode just handles module registration
    // UI interaction is handled by debug interface
    
    // Optional: periodic status updates
    static unsigned long lastStatusUpdate = 0;
    unsigned long now = millis();
    if (now - lastStatusUpdate > 10000) { // Every 10 seconds
        Serial.print("Discovery: Active for ");
        Serial.print((now - discovery_start_time) / 1000);
        Serial.print("s, modules found: ");
        Serial.println(modules.size());
        lastStatusUpdate = now;
    }
}

unsigned long GameStateManager::getDiscoveryDuration() const {
    if (!isInDiscoveryMode()) return 0;
    return millis() - discovery_start_time;
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
    uint8_t serialCmd[7];
    serialCmd[0] = SERIAL_DISPLAY_SET_SERIAL;
    memcpy(&serialCmd[1], serialNumber.c_str(), 6);
    sendCanMessage(CAN_ID_SERIAL_DISPLAY, serialCmd, 7);
    Serial.print("GameState: Sent serial number to display: ");
    Serial.println(serialNumber);
}

void GameStateManager::startGame() {
    if (currentState != GameState::IDLE) {
        Serial.println("GameState: Cannot start game - not in IDLE state");
        return;
    }
    
    Serial.println("GameState: Starting game...");
    
    // First, broadcast game start to all modules
    Serial.print("GameState: Broadcasting GAME_START to ");
    Serial.print(modules.size());
    Serial.println(" modules...");
    
    for (const auto& module : modules) {
        if (module.category == ModuleCategory::REGULAR) {
            // Send full game state to each module
            broadcastGameState(module.canId);
            
            // Send game start message
            uint8_t gameStartData[1] = {TIMER_GAME_START};
            sendCanMessage(module.canId, gameStartData, 1);
        }
    }
    
    // Also broadcast to all modules via broadcast ID
    uint8_t gameStartData[1] = {TIMER_GAME_START};
    sendCanMessage(CAN_ID_BROADCAST, gameStartData, 1);
    
    // Then start the timer
    setState(GameState::RUNNING);
    startTimer();
    
    Serial.println("GameState: Game started successfully!");
}

// Debug function removed

 