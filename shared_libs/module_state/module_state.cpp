#include "module_state.h"
#include <can_bus.h>
#include <Arduino.h>

// Global instance for convenience functions
ModuleState* globalModuleState = nullptr;

// ============================================================================
// CONSTRUCTOR
// ============================================================================

ModuleState::ModuleState()
    : isDiscovered(false), isRegistered(false), lastRegisterAttempt(0),
      lastDiscoveryFlashTime(0), lastHeartbeat(0), gameRunning(false),
      currentStatus(MODULE_STATUS_IDLE), progress(0), solved(false),
      moduleId(0xFFFF), enabled(false), communicationsEnabled(true), statusDirty(false),
      statusLedPin(MODULE_STATE_NO_LED),
      discoveryLedState(false), manualLedState(false), manualLedOverride(false),
      currentStrikes(0), strikeFlashActive(false), strikeFlashStart(0),
      lastStrikeCount(0), hasSerialFirstHalf(false), edgeworkReceived(false),
      gameStateCallback(nullptr), strikeCallback(nullptr),
      serialNumberCallback(nullptr), edgeworkCallback(nullptr),
      discoveredCallback(nullptr) {
    serialNumberFirstHalf[0] = '\0';
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ModuleState::begin(int statusLedPinParam) {
    enabled = true;
    lastHeartbeat = millis();
    lastRegisterAttempt = millis();
    setLedPin(statusLedPinParam);
    refreshModuleId();
    statusDirty = true;
    
    // Send initial registration
    sendRegisterNow();
}

void ModuleState::update() {
    if (!enabled) return;

    refreshModuleId();
    
    if (statusDirty && isDiscovered && communicationsEnabled) {
        sendStatusUpdate();
    }
    
    unsigned long now = millis();
    
    // Send registration if not discovered
    if (communicationsEnabled && !isDiscovered &&
        (now - lastRegisterAttempt >= MODULE_STATE_REGISTER_INTERVAL)) {
        sendRegister();
    }
    
    // Send heartbeat at appropriate interval
    if (communicationsEnabled &&
        (now - lastHeartbeat >= getCurrentHeartbeatInterval())) {
        sendHeartbeat();
    }
    
    // Update LED state (manual override takes precedence)
    if (statusLedPin != MODULE_STATE_NO_LED) {
        if (manualLedOverride) {
            digitalWrite(statusLedPin, manualLedState ? HIGH : LOW);
        } else if (strikeFlashActive) {
            // Strike flash red - handled by updateStrikeLed()
            updateStrikeLed();
        } else if (solved) {
            // Solved: Solid green
            digitalWrite(statusLedPin, HIGH);
        } else if (!isDiscovered) {
            // Not discovered: Flash green (500ms on/off)
            updateDiscoveryLed();
        } else if (isDiscovered && !gameRunning) {
            // Discovered but game not running: Solid green
            digitalWrite(statusLedPin, HIGH);
        } else if (isDiscovered && gameRunning) {
            // Game running: LED off
            digitalWrite(statusLedPin, LOW);
        }
    }
}

// ============================================================================
// CAN MESSAGE HANDLING
// ============================================================================

void ModuleState::handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    // Only process messages from timer
    // can_bus already filters by destination (id == thisModuleId || id == CAN_ID_BROADCAST)
    // So we just need to verify the sender is the timer
    if (senderId != CAN_ID_TIMER) {
        return;
    }
    
    if (len < 1) {
        return;
    }
    
    uint8_t msgType = data[0];
    handleTimerMessage(msgType, data, len);
}

void ModuleState::handleTimerMessage(uint8_t msgType, const uint8_t* data, uint8_t len) {
    switch (msgType) {
        case TIMER_MODULE_DISCOVERED:
            if (!isDiscovered) {
                isDiscovered = true;
                isRegistered = true;
                discoveryLedState = false;
                statusDirty = true;
                
                // Turn off discovery LED
                if (statusLedPin != MODULE_STATE_NO_LED) {
                    digitalWrite(statusLedPin, LOW);
                }
                
                // Call callback if set
                if (discoveredCallback) {
                    discoveredCallback();
                }
            }
            break;
            
        case TIMER_GAME_START:
            if (!gameRunning) {
                gameRunning = true;
                
                // Send immediate heartbeat on state change
                sendHeartbeatNow();
                
                // Call callback if set
                if (gameStateCallback) {
                    gameStateCallback(true);
                }
            }
            break;
            
        case TIMER_GAME_STOP:
            if (gameRunning) {
                gameRunning = false;
                
                // Send immediate heartbeat on state change
                sendHeartbeatNow();
                
                // Call callback if set
                if (gameStateCallback) {
                    gameStateCallback(false);
                }
            }
            break;
            
        case TIMER_STRIKES:
            if (len >= 2) {
                uint8_t strikes = data[1];
                if (strikes != currentStrikes) {
                    // Check if this is a new strike (count increased)
                    if (strikes > currentStrikes) {
                        // Trigger strike flash
                        strikeFlashActive = true;
                        strikeFlashStart = millis();
                        
                        if (statusLedPin != MODULE_STATE_NO_LED) {
                            digitalWrite(statusLedPin, HIGH);  // Flash red (HIGH = on)
                        }

                    }
                    
                    currentStrikes = strikes;
                    lastStrikeCount = strikes;
                    
                    // Call callback if set
                    if (strikeCallback) {
                        strikeCallback(strikes);
                    }
                }
            }
            break;
            
        case TIMER_SERIAL_NUMBER_FIRST_HALF:
            if (len >= 4) {
                // Store first 3 characters
                memcpy(serialNumberFirstHalf, &data[1], 3);
                serialNumberFirstHalf[3] = '\0';
                hasSerialFirstHalf = true;
            }
            break;
            
        case TIMER_SERIAL_NUMBER_LAST_HALF:
            if (len >= 4 && hasSerialFirstHalf) {
                // Combine first and last halves
                char serial[7];
                memcpy(serial, serialNumberFirstHalf, 3);
                memcpy(&serial[3], &data[1], 3);
                serial[6] = '\0';
                String newSerial = String(serial);
                
                if (newSerial != serialNumber) {
                    serialNumber = newSerial;
                    hasSerialFirstHalf = false;  // Reset for next time
                    
                    // Call callback if set
                    if (serialNumberCallback) {
                        serialNumberCallback(serialNumber);
                    }
                } else {
                    hasSerialFirstHalf = false;  // Reset for next time
                }
            }
            break;
            
        case TIMER_RESET:
            // Reset discovery state (will re-register)
            isDiscovered = false;
            isRegistered = false;
            gameRunning = false;
            currentStrikes = 0;
            lastStrikeCount = 0;
            solved = false;
            currentStatus = MODULE_STATUS_IDLE;
            progress = 0;
            serialNumber = "";
            serialNumberFirstHalf[0] = '\0';
            hasSerialFirstHalf = false;
            edgework.clear();
            edgeworkReceived = false;
            strikeFlashActive = false;
            manualLedOverride = false;
            statusDirty = true;
            
            // Send registration immediately
            sendRegisterNow();
            
            // Call game state callback if set
            if (gameStateCallback) {
                gameStateCallback(false);
            }
            break;
            
        case TIMER_TIME:
            // Time updates are passed through but not stored by module_state
            // Modules can handle this in their own message handlers if needed
            break;
            
        case TIMER_COUNTDOWN:
            // Countdown updates are passed through but not stored by module_state
            // Modules can handle this in their own message handlers if needed
            break;
            
        default:
            // Unknown message type - pass through to module's handler
            break;
    }
}

// ============================================================================
// DISCOVERY & REGISTRATION
// ============================================================================

void ModuleState::sendRegister() {
    if (!communicationsEnabled) {
        return;
    }
    
    if (isRegistered) {
        return;  // Already registered
    }
    
    uint8_t registerData[1];
    registerData[0] = MODULE_REGISTER;
    
    sendCanFrame(CAN_ID_TIMER, registerData, 1);
    lastRegisterAttempt = millis();
    statusDirty = true;
}

void ModuleState::sendRegisterNow() {
    isRegistered = false;  // Force re-registration
    sendRegister();
}

// ============================================================================
// HEARTBEAT MANAGEMENT
// ============================================================================

unsigned long ModuleState::getCurrentHeartbeatInterval() const {
    // Use discovery interval if not discovered, otherwise use game interval
    if (!isDiscovered || !gameRunning) {
        return MODULE_STATE_HEARTBEAT_INTERVAL_DISCOVERY;
    } else {
        return MODULE_STATE_HEARTBEAT_INTERVAL_GAME;
    }
}

void ModuleState::sendHeartbeat() {
    if (!enabled || !communicationsEnabled) {
        return;
    }
    
    // Simple heartbeat format: [MODULE_HEARTBEAT]
    // Enhanced format: [MODULE_HEARTBEAT, status, solved_flag, progress]
    uint8_t heartbeatData[4];
    heartbeatData[0] = MODULE_HEARTBEAT;
    heartbeatData[1] = currentStatus;
    heartbeatData[2] = solved ? 1 : 0;
    heartbeatData[3] = progress;
    
    sendCanFrame(CAN_ID_TIMER, heartbeatData, 4);
    lastHeartbeat = millis();
}

void ModuleState::sendHeartbeatNow() {
    sendHeartbeat();
}

// ============================================================================
// STATUS LED CONTROL
// ============================================================================

void ModuleState::updateDiscoveryLed() {
    if (statusLedPin == MODULE_STATE_NO_LED) {
        return;
    }
    
    unsigned long now = millis();
    if (now - lastDiscoveryFlashTime >= MODULE_STATE_DISCOVERY_LED_INTERVAL) {
        discoveryLedState = !discoveryLedState;
        digitalWrite(statusLedPin, discoveryLedState ? HIGH : LOW);
        lastDiscoveryFlashTime = now;
    }
}

void ModuleState::updateStrikeLed() {
    if (statusLedPin == MODULE_STATE_NO_LED) {
        return;
    }
    
    unsigned long now = millis();
    if (now - strikeFlashStart >= MODULE_STATE_STRIKE_FLASH_DURATION) {
        strikeFlashActive = false;
        digitalWrite(statusLedPin, LOW);  // Turn off after flash
    }
}

void ModuleState::setLedPin(int pin) {
    if (pin == statusLedPin) {
        return;
    }

    if (pin == MODULE_STATE_NO_LED) {
        disableLed();
        return;
    }

    if (statusLedPin != MODULE_STATE_NO_LED) {
        digitalWrite(statusLedPin, LOW);
        pinMode(statusLedPin, INPUT);
    }

    statusLedPin = pin;
    pinMode(pin, OUTPUT);
    digitalWrite(pin, LOW);
    manualLedOverride = false;
    strikeFlashActive = false;
    discoveryLedState = false;
}

void ModuleState::setLedState(bool state) {
    manualLedState = state;
    manualLedOverride = true;
    if (statusLedPin != MODULE_STATE_NO_LED) {
        digitalWrite(statusLedPin, state ? HIGH : LOW);
    }
}

void ModuleState::clearLedOverride() {
    manualLedOverride = false;
    if (statusLedPin != MODULE_STATE_NO_LED && !strikeFlashActive) {
        digitalWrite(statusLedPin, LOW);
    }
}

void ModuleState::disableLed() {
    if (statusLedPin != MODULE_STATE_NO_LED) {
        digitalWrite(statusLedPin, LOW);
        pinMode(statusLedPin, INPUT);
    }

    statusLedPin = MODULE_STATE_NO_LED;
    manualLedOverride = false;
    strikeFlashActive = false;
    discoveryLedState = false;
}

void ModuleState::setCommunicationEnabled(bool enabledFlag) {
    communicationsEnabled = enabledFlag;
    if (communicationsEnabled) {
        statusDirty = true;
        sendStatusUpdate(true);
    }
}

void ModuleState::setDiscovered(bool discovered) {
    if (isDiscovered == discovered) {
        return;
    }
    
    isDiscovered = discovered;
    isRegistered = discovered;
    
    if (!discovered) {
        discoveryLedState = false;
    } else if (discoveredCallback) {
        discoveredCallback();
    }
}

void ModuleState::setGameRunning(bool running) {
    if (gameRunning == running) {
        return;
    }
    
    gameRunning = running;
    
    if (gameStateCallback) {
        gameStateCallback(running);
    }
}

void ModuleState::setStrikeCount(uint8_t strikeCount) {
    if (currentStrikes == strikeCount) {
        return;
    }
    
    currentStrikes = strikeCount;
    lastStrikeCount = strikeCount;
    strikeFlashActive = false;
    
    if (strikeCallback) {
        strikeCallback(strikeCount);
    }
}

void ModuleState::setSerialNumber(const String& serial) {
    if (serialNumber == serial) {
        return;
    }
    
    serialNumber = serial;
    hasSerialFirstHalf = false;
    
    if (serialNumberCallback) {
        serialNumberCallback(serialNumber);
    }
}

void ModuleState::setEdgework(const Edgework& edgeworkData) {
    edgework = edgeworkData;
    edgeworkReceived = true;
    
    if (edgeworkCallback) {
        edgeworkCallback(edgework);
    }
}

// ============================================================================
// STATUS MANAGEMENT
// ============================================================================

void ModuleState::setStatus(ModuleStatus status) {
    if (currentStatus != status) {
        currentStatus = status;
        statusDirty = true;
        sendStatusUpdate();
    }
}

void ModuleState::setProgress(uint8_t progressPercent) {
    uint8_t clamped = min(progressPercent, (uint8_t)100);
    if (progress != clamped) {
        progress = clamped;
        statusDirty = true;
        sendStatusUpdate();
    }
}

void ModuleState::setSolved(bool solved) {
    if (this->solved != solved) {
        this->solved = solved;
        
        // Send MODULE_SOLVED message when solved
        if (solved) {
            uint8_t solvedData[1];
            solvedData[0] = MODULE_SOLVED;
            sendCanFrame(CAN_ID_TIMER, solvedData, 1);
            
            // Play solved sound
            playAudio(AUDIO_DEFUSED);
            
            // Update status
            currentStatus = MODULE_STATUS_SOLVED;
        } else if (currentStatus == MODULE_STATUS_SOLVED) {
            currentStatus = MODULE_STATUS_IDLE;
        }
        
        statusDirty = true;
        sendStatusUpdate(true);
    }
}

// ============================================================================
// STRIKE HANDLING
// ============================================================================

void ModuleState::triggerStrike() {
    // Send strike message to timer
    uint8_t strikeData[1];
    strikeData[0] = MODULE_STRIKE;
    sendCanFrame(CAN_ID_TIMER, strikeData, 1);
}

void ModuleState::refreshModuleId() {
    uint16_t currentId = ::getCurrentModuleId();
    if (currentId != 0 && currentId != 0xFFFF) {
        moduleId = currentId;
    }
}

bool ModuleState::sendCanFrame(uint16_t receiverId, const uint8_t* data, uint8_t len) {
    if (!enabled || !communicationsEnabled || data == nullptr) {
        return false;
    }

    refreshModuleId();
    sendCanMessage(receiverId, data, len);
    return true;
}

bool ModuleState::sendStatusUpdate(bool force) {
    if (!enabled || !communicationsEnabled) {
        return false;
    }
    
    if (!isDiscovered) {
        return false;
    }
    
    if (!force && !statusDirty) {
        return true;
    }
    
    uint8_t statusData[4];
    statusData[0] = MODULE_STATUS;
    statusData[1] = currentStatus;
    statusData[2] = solved ? 1 : 0;
    statusData[3] = progress;
    
    bool sent = sendCanFrame(CAN_ID_TIMER, statusData, 4);
    if (sent) {
        statusDirty = false;
    }
    return sent;
}

uint16_t ModuleState::getModuleId() const {
    if (moduleId != 0xFFFF && moduleId != 0) {
        return moduleId;
    }
    return ::getCurrentModuleId();
}

uint8_t ModuleState::getModuleInstanceId() const {
    return getModuleId() & 0x1F;
}

bool ModuleState::sendMessage(uint16_t receiverId, const uint8_t* data, uint8_t len) {
    return sendCanFrame(receiverId, data, len);
}

bool ModuleState::sendTimerMessage(const uint8_t* data, uint8_t len) {
    return sendCanFrame(CAN_ID_TIMER, data, len);
}

bool ModuleState::sendBroadcastMessage(const uint8_t* data, uint8_t len) {
    return sendCanFrame(CAN_ID_BROADCAST, data, len);
}

bool ModuleState::playAudio(CanAudioSound sound) {
    uint8_t payload = static_cast<uint8_t>(sound);
    return sendCanFrame(CAN_ID_AUDIO, &payload, 1);
}

bool ModuleState::playAudio(const uint8_t* soundCodes, uint8_t len) {
    if (soundCodes == nullptr || len == 0 || len > MAX_CAN_PAYLOAD) {
        return false;
    }
    return sendCanFrame(CAN_ID_AUDIO, soundCodes, len);
}

bool ModuleState::sendTelemetry(uint8_t telemetryType, const uint8_t* payload, uint8_t len) {
    if (len > (MAX_CAN_PAYLOAD - 2)) {
        return false;
    }
    
    uint8_t buffer[MAX_CAN_PAYLOAD];
    buffer[0] = MODULE_STATUS;
    buffer[1] = MODULE_TELEMETRY_FLAG | (telemetryType & 0x7F);
    
    if (len > 0) {
        if (payload == nullptr) {
            return false;
        }
        memcpy(&buffer[2], payload, len);
    }
    
    return sendCanFrame(CAN_ID_TIMER, buffer, len + 2);
}

// ============================================================================
// GLOBAL CONVENIENCE FUNCTIONS
// ============================================================================

void initModuleState(int statusLedPin) {
    static ModuleState instance;
    globalModuleState = &instance;
    globalModuleState->begin(statusLedPin);
}

void updateModuleState() {
    if (globalModuleState) {
        globalModuleState->update();
    }
}

void setModuleStateStatus(ModuleStatus status) {
    if (globalModuleState) {
        globalModuleState->setStatus(status);
    }
}

void setModuleStateProgress(uint8_t progress) {
    if (globalModuleState) {
        globalModuleState->setProgress(progress);
    }
}

void setModuleStateSolved(bool solved) {
    if (globalModuleState) {
        globalModuleState->setSolved(solved);
    }
}

void moduleStateHandleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    if (globalModuleState) {
        globalModuleState->handleCanMessage(id, senderId, data, len);
    }
}

void setModuleStateLedPin(int pin) {
    if (globalModuleState) {
        globalModuleState->setLedPin(pin);
    }
}

void setModuleStateLedState(bool state) {
    if (globalModuleState) {
        globalModuleState->setLedState(state);
    }
}

void clearModuleStateLedOverride() {
    if (globalModuleState) {
        globalModuleState->clearLedOverride();
    }
}

void disableModuleStateLed() {
    if (globalModuleState) {
        globalModuleState->disableLed();
    }
}

void setModuleStateCommunicationEnabled(bool enabled) {
    if (globalModuleState) {
        globalModuleState->setCommunicationEnabled(enabled);
    }
}

void setModuleStateDiscovered(bool discovered) {
    if (globalModuleState) {
        globalModuleState->setDiscovered(discovered);
    }
}

void setModuleStateGameRunning(bool running) {
    if (globalModuleState) {
        globalModuleState->setGameRunning(running);
    }
}

void setModuleStateStrikeCount(uint8_t strikeCount) {
    if (globalModuleState) {
        globalModuleState->setStrikeCount(strikeCount);
    }
}

void setModuleStateSerialNumber(const String& serial) {
    if (globalModuleState) {
        globalModuleState->setSerialNumber(serial);
    }
}

void setModuleStateEdgework(const Edgework& edgeworkData) {
    if (globalModuleState) {
        globalModuleState->setEdgework(edgeworkData);
    }
}

