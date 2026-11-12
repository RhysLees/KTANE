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
    sendCanMessage(canId, soundData, 1);
}

void AudioModule::setVolume(uint8_t volumePercent) {
    uint8_t clamped = volumePercent > 100 ? 100 : volumePercent;
    uint8_t payload[2] = {AUDIO_SET_VOLUME, clamped};
    sendCanMessage(canId, payload, 2);
}

void AudioModule::markSeen() {
    connected = true;
    lastSeen = millis();
}

void AudioModule::markDisconnected() {
    connected = false;
}

void AudioModule::updateStatus(uint8_t moduleStatus, uint8_t moduleProgress, bool moduleSolved) {
    status = moduleStatus;
    progress = moduleProgress;
    solved = moduleSolved;
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
        sendCanMessage(canId, buf, 7);
    }
}

void SerialModule::clear() {
    uint8_t buf[1] = {SERIAL_DISPLAY_CLEAR};
    sendCanMessage(canId, buf, 1);
}

void SerialModule::showCredit() {
    uint8_t buf[1] = {SERIAL_DISPLAY_SHOW_CREDIT};
    sendCanMessage(canId, buf, 1);
}

void SerialModule::markSeen() {
    connected = true;
    lastSeen = millis();
}

void SerialModule::markDisconnected() {
    connected = false;
}

void SerialModule::updateStatus(uint8_t moduleStatus, uint8_t moduleProgress, bool moduleSolved) {
    status = moduleStatus;
    progress = moduleProgress;
    solved = moduleSolved;
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
    
    audioModules.clear();
    audioModuleMap.clear();
    serialModules.clear();
    serialModuleMap.clear();
    
    // Reserve capacity upfront to prevent pointer invalidation on reallocation
    modules.reserve(32);  // Reserve for up to 32 modules
    audioModules.reserve(1);   // Reserve for audio modules
    serialModules.reserve(1);   // Reserve for serial modules
    
    generateSerialNumber();
    setupEdgework();
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
        strikeData[0] = TIMER_STRIKES;
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
    // Route AUDIO and SERIAL_DISPLAY to their respective arrays instead of modules
    // Check by CAN ID first (more reliable than type comparison due to CAN type vs ModuleType mismatch)
    if (canId == CAN_ID_AUDIO || type == ModuleType::AUDIO) {
        registerAudioModule(canId);
        return;
    }
    if (canId == CAN_ID_SERIAL_DISPLAY || type == ModuleType::SERIAL_DISPLAY) {
        registerSerialModule(canId);
        return;
    }
    
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

void GameStateManager::registerAudioModule(uint16_t canId) {
    if (audioModuleMap.find(canId) != audioModuleMap.end()) {
        return;
    }
    
    // Reserve capacity to prevent reallocation (which would invalidate pointers in audioModuleMap)
    if (audioModules.capacity() < 1) {
        audioModules.reserve(1);
    }
    
    audioModules.emplace_back(canId);
    AudioModule* newModule = &audioModules.back();
    audioModuleMap[canId] = newModule;
    
    newModule->setVolume(audioVolume);
}

void GameStateManager::unregisterAudioModule(uint16_t canId) {
    auto it = audioModuleMap.find(canId);
    if (it != audioModuleMap.end()) {
        // Remove from vector
        auto vecIt = std::find_if(audioModules.begin(), audioModules.end(),
            [canId](const AudioModule& m) { return m.canId == canId; });
        if (vecIt != audioModules.end()) {
            audioModules.erase(vecIt);
        }
        audioModuleMap.erase(it);
    }
}

AudioModule* GameStateManager::getAudioModule(uint16_t canId) {
    auto it = audioModuleMap.find(canId);
    return it != audioModuleMap.end() ? it->second : nullptr;
}

const AudioModule* GameStateManager::getAudioModule(uint16_t canId) const {
    auto it = audioModuleMap.find(canId);
    return it != audioModuleMap.end() ? it->second : nullptr;
}

AudioModule* GameStateManager::getAudioModule() {
    return getAudioModule(CAN_ID_AUDIO);
}

const AudioModule* GameStateManager::getAudioModule() const {
    return getAudioModule(CAN_ID_AUDIO);
}

void GameStateManager::updateAudioModuleSeen(uint16_t canId) {
    AudioModule* module = getAudioModule(canId);
    if (module) {
        module->markSeen();
    }
}

void GameStateManager::setAudioVolume(uint8_t volumePercent) {
    uint8_t clamped = volumePercent > 100 ? 100 : volumePercent;
    audioVolume = clamped;

    bool sentToRegistered = false;
    for (auto& module : audioModules) {
        module.setVolume(clamped);
        sentToRegistered = true;
    }

    if (!sentToRegistered) {
        uint8_t payload[2] = {AUDIO_SET_VOLUME, clamped};
        sendCanMessage(CAN_ID_AUDIO, payload, 2);
    }
}

void GameStateManager::registerSerialModule(uint16_t canId) {
    if (serialModuleMap.find(canId) != serialModuleMap.end()) {
        return;
    }
    
    // Reserve capacity to prevent reallocation (which would invalidate pointers in serialModuleMap)
    if (serialModules.capacity() < 1) {
        serialModules.reserve(1);
    }
    
    serialModules.emplace_back(canId);
    SerialModule* newModule = &serialModules.back();
    serialModuleMap[canId] = newModule;
    
    // Send stored serial number to newly registered serial module
    // Use both methods: direct setSerialNumber (for backward compatibility) 
    // and broadcastGameState (for module_state compatibility)
    if (serialNumber.length() == 6) {
        // Method 1: Direct setSerialNumber (sends SERIAL_DISPLAY_SET_SERIAL)
        newModule->setSerialNumber(serialNumber);
        // Method 2: broadcastGameState (sends TIMER_SERIAL_NUMBER for module_state)
        broadcastGameState(canId);
    }
}

void GameStateManager::unregisterSerialModule(uint16_t canId) {
    auto it = serialModuleMap.find(canId);
    if (it != serialModuleMap.end()) {
        // Remove from vector
        auto vecIt = std::find_if(serialModules.begin(), serialModules.end(),
            [canId](const SerialModule& m) { return m.canId == canId; });
        if (vecIt != serialModules.end()) {
            serialModules.erase(vecIt);
        }
        serialModuleMap.erase(it);
    }
}

SerialModule* GameStateManager::getSerialModule(uint16_t canId) {
    auto it = serialModuleMap.find(canId);
    return it != serialModuleMap.end() ? it->second : nullptr;
}

const SerialModule* GameStateManager::getSerialModule(uint16_t canId) const {
    auto it = serialModuleMap.find(canId);
    return it != serialModuleMap.end() ? it->second : nullptr;
}

SerialModule* GameStateManager::getSerialModule() {
    return getSerialModule(CAN_ID_SERIAL_DISPLAY);
}

const SerialModule* GameStateManager::getSerialModule() const {
    return getSerialModule(CAN_ID_SERIAL_DISPLAY);
}

void GameStateManager::updateSerialModuleSeen(uint16_t canId) {
    SerialModule* module = getSerialModule(canId);
    if (module) {
        module->markSeen();
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

void GameStateManager::ingestModuleStatus(uint16_t canId, const uint8_t* data, uint8_t len, bool sendDiscoveryAck) {
    Module* module = getModule(canId);
    bool newlyRegistered = false;
    
    if (!module) {
        uint8_t moduleType = (canId >> 5) & 0x3F;
        registerModule(canId, static_cast<ModuleType>(moduleType));
        module = getModule(canId);
        newlyRegistered = (module != nullptr);
        
        if (newlyRegistered && sendDiscoveryAck) {
            uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
            sendCanMessage(canId, discoveryAck, 1);
            broadcastGameState(canId);
        }
    }
    
    if (!module) {
        return;
    }
    
    updateModuleSeen(canId);
    unsigned long now = millis();
    
    if (len >= 2 && (data[1] & MODULE_TELEMETRY_FLAG)) {
        module->hasTelemetry = true;
        module->telemetryType = data[1] & 0x7F;
        module->telemetryLen = min<uint8_t>(len - 2, (uint8_t)sizeof(module->telemetryData));
        if (module->telemetryLen > 0) {
            memcpy(module->telemetryData, &data[2], module->telemetryLen);
        }
        module->lastTelemetryUpdate = now;

        if (module->telemetryType == MODULE_TELEMETRY_GENERAL) {
            if (module->telemetryLen >= 1) {
                module->status = static_cast<ModuleStatus>(module->telemetryData[0]);
            }
            if (module->telemetryLen >= 2) {
                bool solvedFlag = module->telemetryData[1] != 0;
                if (solvedFlag) {
                    module->isSolved = true;
                }
            }
            if (module->telemetryLen >= 3) {
                module->progress = min<uint8_t>(module->telemetryData[2], (uint8_t)100);
            }
            module->lastStatusUpdate = now;
        }
        return;
    }
    
    if (len >= 4) {
        module->status = static_cast<ModuleStatus>(data[1]);
        bool solvedFlag = (data[2] != 0);
        module->progress = min<uint8_t>(data[3], (uint8_t)100);
        module->lastStatusUpdate = now;
        if (solvedFlag) {
            module->isSolved = true;
        }
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
    // Store serial number in GameStateManager
    serialNumber = serial.substring(0, 6);
    
    // Send to serial module if it exists
    SerialModule* serialModule = getSerialModule();
    if (serialModule) {
        serialModule->setSerialNumber(serialNumber);
    }
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
    
    // Store serial number in GameStateManager
    serialNumber = String(serialBuf);
    
    // Send to serial module if it exists
    SerialModule* serialModule = getSerialModule();
    if (serialModule) {
        // Method 1: Direct setSerialNumber (sends SERIAL_DISPLAY_SET_SERIAL)
        serialModule->setSerialNumber(serialNumber);
        // Method 2: broadcastGameState (sends TIMER_SERIAL_NUMBER for module_state)
        broadcastGameState(serialModule->canId);
    }
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
    unsigned long timeout = GAME_STATE_MODULE_INACTIVE_TIMEOUT_MS;
    
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
            // Check if already registered (in modules or system arrays)
            bool alreadyRegistered = (moduleMap.find(senderId) != moduleMap.end()) ||
                                     (audioModuleMap.find(senderId) != audioModuleMap.end()) ||
                                     (serialModuleMap.find(senderId) != serialModuleMap.end());
            
            if (!alreadyRegistered) {
                uint8_t moduleType = (senderId >> 5) & 0x3F;
                registerModule(senderId, static_cast<ModuleType>(moduleType));
                uint8_t decodedModuleType, decodedInstanceId;
                decodeCanId(senderId, &decodedModuleType, &decodedInstanceId);
                
                // Mark audio or serial module as seen if they're registering
                if (senderId == CAN_ID_AUDIO) {
                    updateAudioModuleSeen(senderId);
                } else if (senderId == CAN_ID_SERIAL_DISPLAY) {
                    updateSerialModuleSeen(senderId);
                }
                
                // Send module discovered acknowledgment (message 1)
                uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
                sendCanMessage(senderId, discoveryAck, 1);
                
                // Send current state to newly registered module (messages 2-5)
                // Order: TIMER_SERIAL_NUMBER_FIRST_HALF, TIMER_SERIAL_NUMBER_LAST_HALF, TIMER_STRIKES, TIMER_TIME
                broadcastGameState(senderId);
            }
            
            break;
        }
            
        case MODULE_STRIKE:
            addStrike();
            // Strike callback will handle broadcasting
            break;
            
        case MODULE_SOLVED:
            setModuleSolved(senderId);
            // Module solved callback will handle broadcasting
            break;
            
        case MODULE_STATUS:
            // Update audio or serial module seen time
            if (senderId == CAN_ID_AUDIO) {
                updateAudioModuleSeen(senderId);
                // Handle additional status data if available
                if (len >= 2 && (data[1] & MODULE_TELEMETRY_FLAG)) {
                    // Reserved for future audio telemetry handling
                } else if (len >= 4) {
                    uint8_t moduleStatus = data[1];
                    bool moduleSolved = (data[2] != 0);
                    uint8_t moduleProgress = data[3];
                    AudioModule* audioModule = getAudioModule(senderId);
                    if (audioModule) {
                        audioModule->updateStatus(moduleStatus, moduleProgress, moduleSolved);
                    }
                }
            } else if (senderId == CAN_ID_SERIAL_DISPLAY) {
                updateSerialModuleSeen(senderId);
                // Handle additional status data if available
                if (len >= 2 && (data[1] & MODULE_TELEMETRY_FLAG)) {
                    // Reserved for future serial telemetry handling
                } else if (len >= 4) {
                    uint8_t moduleStatus = data[1];
                    bool moduleSolved = (data[2] != 0);
                    uint8_t moduleProgress = data[3];
                    SerialModule* serialModule = getSerialModule(senderId);
                    if (serialModule) {
                        serialModule->updateStatus(moduleStatus, moduleProgress, moduleSolved);
                    }
                }
            } else {
                ingestModuleStatus(senderId, data, len, true);
            }
            break;
            
        case MODULE_HEARTBEAT:
            // Update last seen time for known modules
            if (senderId == CAN_ID_AUDIO) {
                updateAudioModuleSeen(senderId);
                // Process enhanced heartbeat data if available
                if (len >= 2 && (data[1] & MODULE_TELEMETRY_FLAG)) {
                    // Reserved for future audio telemetry via heartbeat
                } else if (len >= 4) {
                    uint8_t moduleStatus = data[1];
                    bool moduleSolved = (data[2] != 0);
                    uint8_t moduleProgress = data[3];
                    AudioModule* audioModule = getAudioModule(senderId);
                    if (audioModule) {
                        audioModule->updateStatus(moduleStatus, moduleProgress, moduleSolved);
                    }
                }
            } else if (senderId == CAN_ID_SERIAL_DISPLAY) {
                updateSerialModuleSeen(senderId);
                // Process enhanced heartbeat data if available
                if (len >= 2 && (data[1] & MODULE_TELEMETRY_FLAG)) {
                    // Reserved for future serial telemetry via heartbeat
                } else if (len >= 4) {
                    uint8_t moduleStatus = data[1];
                    bool moduleSolved = (data[2] != 0);
                    uint8_t moduleProgress = data[3];
                    SerialModule* serialModule = getSerialModule(senderId);
                    if (serialModule) {
                        serialModule->updateStatus(moduleStatus, moduleProgress, moduleSolved);
                    }
                }
            } else {
                // Auto-register module if not already registered (discovery via heartbeat)
                // Check all maps (modules and system arrays)
                if (moduleMap.find(senderId) == moduleMap.end() &&
                    audioModuleMap.find(senderId) == audioModuleMap.end() &&
                    serialModuleMap.find(senderId) == serialModuleMap.end()) {
                    uint8_t moduleType = (senderId >> 5) & 0x3F;
                    registerModule(senderId, static_cast<ModuleType>(moduleType));
                    uint8_t decodedModuleType, decodedInstanceId;
                    decodeCanId(senderId, &decodedModuleType, &decodedInstanceId);
                    
                    // Send module discovered acknowledgment
                    uint8_t discoveryAck[1] = {TIMER_MODULE_DISCOVERED};
                    sendCanMessage(senderId, discoveryAck, 1);
                    
                    // Send current state to newly registered module
                    broadcastGameState(senderId);
                }
                
                ingestModuleStatus(senderId, data, len, false);
            }
            break;
            
        // Handle epaper display messages
        case SERIAL_DISPLAY_CLEAR:
            if (senderId == CAN_ID_SERIAL_DISPLAY) {
                updateSerialModuleSeen(senderId);
            }
            break;
            
        default:
            // Unknown message type - ignore
            break;
    }
}

void GameStateManager::broadcastGameState(uint16_t targetId) {
    // Send 5 messages in order:
    // 1. TIMER_MODULE_DISCOVERED (already sent before this function is called)
    // 2. TIMER_SERIAL_NUMBER_FIRST_HALF
    // 3. TIMER_SERIAL_NUMBER_LAST_HALF
    // 4. TIMER_STRIKES
    // 5. TIMER_TIME
    
    // Send serial number in two parts (first 3 chars, then last 3 chars)
    if (serialNumber.length() == 6) {
        // First half: first 3 characters
        uint8_t firstHalf[4];
        firstHalf[0] = TIMER_SERIAL_NUMBER_FIRST_HALF;
        memcpy(&firstHalf[1], serialNumber.c_str(), 3);
        sendCanMessage(targetId, firstHalf, 4);
        
        // Second half: last 3 characters
        uint8_t lastHalf[4];
        lastHalf[0] = TIMER_SERIAL_NUMBER_LAST_HALF;
        memcpy(&lastHalf[1], serialNumber.c_str() + 3, 3);
        sendCanMessage(targetId, lastHalf, 4);
    }
    
    // Send strike count
    uint8_t strikeData[2];
    strikeData[0] = TIMER_STRIKES;
    strikeData[1] = strikeCount;
    sendCanMessage(targetId, strikeData, 2);
    
    // Send time remaining
    uint8_t timeData[5];
    timeData[0] = TIMER_TIME;
    memcpy(&timeData[1], &remainingMs, 4);
    sendCanMessage(targetId, timeData, 5);
}


void GameStateManager::startGame() {
    if (currentState != GameState::IDLE) {
        return;
    }
    
    // Broadcast to all modules via broadcast ID
    uint8_t gameStartData[1] = {TIMER_GAME_START};
    sendCanMessage(CAN_ID_BROADCAST, gameStartData, 1);
    
    // Then start the timer
    setState(GameState::RUNNING);
    startTimer();
}

// ============================================================================
// MODULE CONNECTION TRACKING
// ============================================================================

void GameStateManager::updateModuleConnections() {
    unsigned long now = millis();
    
    // Determine timeout based on game state (two missed heartbeats by default)
    unsigned long timeout = (currentState == GameState::RUNNING || currentState == GameState::PAUSED) 
        ? GAME_STATE_HEARTBEAT_GAME_TIMEOUT_MS 
        : GAME_STATE_HEARTBEAT_DISCOVERY_TIMEOUT_MS;
    
    // Check audio modules
    for (auto& audioModule : audioModules) {
        if (audioModule.isConnected() && 
            audioModule.getTimeSinceLastSeen() > timeout) {
            audioModule.markDisconnected();
        }
    }
    
    // Check serial modules
    for (auto& serialModule : serialModules) {
        if (serialModule.isConnected() && 
            serialModule.getTimeSinceLastSeen() > timeout) {
            serialModule.markDisconnected();
        }
    }
    
    // Check all registered modules
    // Only remove modules if game is NOT running or paused
    bool canRemoveModules = (currentState != GameState::RUNNING && currentState != GameState::PAUSED);
    
    std::vector<uint16_t> modulesToRemove;
    
    for (auto& module : modules) {
        if (module.lastSeen > 0 && (now - module.lastSeen) > timeout) {
            // Module hasn't been seen recently
            module.isActive = false;
            
            // Remove module if game is not running or paused
            if (canRemoveModules) {
                modulesToRemove.push_back(module.canId);
            }
        }
    }
    
    // Remove modules that timed out (only if game not running/paused)
    for (uint16_t canId : modulesToRemove) {
        unregisterModule(canId);
    }
}

bool GameStateManager::isModuleConnected(uint16_t canId) const {
    // Check audio module
    const AudioModule* audioModule = getAudioModule(canId);
    if (audioModule) {
        return audioModule->isConnected();
    }
    
    // Check serial module
    const SerialModule* serialModule = getSerialModule(canId);
    if (serialModule) {
        return serialModule->isConnected();
    }
    
    // Check registered modules
    auto it = moduleMap.find(canId);
    if (it != moduleMap.end()) {
        const Module* module = it->second;
        if (module && module->lastSeen > 0) {
            unsigned long timeSince = millis() - module->lastSeen;
            return timeSince <= GAME_STATE_MODULE_INACTIVE_TIMEOUT_MS;
        }
    }
    
    return false;
}

unsigned long GameStateManager::getModuleLastSeen(uint16_t canId) const {
    // Check audio module
    const AudioModule* audioModule = getAudioModule(canId);
    if (audioModule) {
        return audioModule->getTimeSinceLastSeen();
    }
    
    // Check serial module
    const SerialModule* serialModule = getSerialModule(canId);
    if (serialModule) {
        return serialModule->getTimeSinceLastSeen();
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
    SerialModule* serialModule = getSerialModule();
    if (serialModule) {
        serialModule->clear();
    }
}

void GameStateManager::showSerialCredit() {
    SerialModule* serialModule = getSerialModule();
    if (serialModule) {
        serialModule->showCredit();
    }
}

String GameStateManager::getSerialNumber() const {
    // Return stored serial number if available
    if (serialNumber.length() == 6) {
        return serialNumber;
    }
    
    // Fallback to serial module if no stored serial number
    const SerialModule* serialModule = getSerialModule();
    return serialModule ? serialModule->getSerialNumber() : String("");
}