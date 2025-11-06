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
      enabled(false), statusLedPin(MODULE_STATE_NO_LED),
      discoveryLedState(false), manualLedState(false), manualLedOverride(false),
      currentStrikes(0), strikeFlashActive(false), strikeFlashStart(0),
      lastStrikeCount(0),       edgeworkReceived(false),
      gameStateCallback(nullptr), strikeCallback(nullptr),
      serialNumberCallback(nullptr), edgeworkCallback(nullptr),
      discoveredCallback(nullptr) {
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void ModuleState::begin(int statusLedPin) {
    this->statusLedPin = statusLedPin;
    enabled = true;
    lastHeartbeat = millis();
    lastRegisterAttempt = millis();
    
    // Initialize LED pin if provided
    if (statusLedPin != MODULE_STATE_NO_LED) {
        pinMode(statusLedPin, OUTPUT);
        digitalWrite(statusLedPin, LOW);
    }
    
    // Send initial registration
    sendRegisterNow();
}

void ModuleState::update() {
    if (!enabled) return;
    
    unsigned long now = millis();
    
    // Send registration if not discovered
    if (!isDiscovered && (now - lastRegisterAttempt >= MODULE_STATE_REGISTER_INTERVAL)) {
        sendRegister();
    }
    
    // Send heartbeat at appropriate interval
    if (now - lastHeartbeat >= getCurrentHeartbeatInterval()) {
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
            
        case TIMER_STRIKE_UPDATE:
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
                        
                        // Play strike sound (only if this is a new strike from timer)
                        // Note: Module-initiated strikes already play sound in triggerStrike()
                        uint8_t audioData[1];
                        audioData[0] = AUDIO_STRIKE;
                        sendCanMessage(CAN_ID_AUDIO, audioData, 1);
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
            
        case TIMER_SERIAL_NUMBER:
            if (len >= 7) {
                char serial[7];
                memcpy(serial, &data[1], 6);
                serial[6] = '\0';
                String newSerial = String(serial);
                
                if (newSerial != serialNumber) {
                    serialNumber = newSerial;
                    
                    // Call callback if set
                    if (serialNumberCallback) {
                        serialNumberCallback(serialNumber);
                    }
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
            edgework.clear();
            edgeworkReceived = false;
            strikeFlashActive = false;
            manualLedOverride = false;
            
            // Send registration immediately
            sendRegisterNow();
            
            // Call game state callback if set
            if (gameStateCallback) {
                gameStateCallback(false);
            }
            break;
            
        case TIMER_TIME_UPDATE:
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
    if (isRegistered) {
        return;  // Already registered
    }
    
    uint8_t registerData[1];
    registerData[0] = MODULE_REGISTER;
    
    sendCanMessage(CAN_ID_TIMER, registerData, 1);
    lastRegisterAttempt = millis();
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
    if (!enabled) {
        return;
    }
    
    // Simple heartbeat format: [MODULE_HEARTBEAT]
    // Enhanced format: [MODULE_HEARTBEAT, status, solved_flag, progress]
    uint8_t heartbeatData[4];
    heartbeatData[0] = MODULE_HEARTBEAT;
    heartbeatData[1] = currentStatus;
    heartbeatData[2] = solved ? 1 : 0;
    heartbeatData[3] = progress;
    
    sendCanMessage(CAN_ID_TIMER, heartbeatData, 4);
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
    statusLedPin = pin;
    if (pin != MODULE_STATE_NO_LED) {
        pinMode(pin, OUTPUT);
        digitalWrite(pin, LOW);
    }
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

// ============================================================================
// STATUS MANAGEMENT
// ============================================================================

void ModuleState::setStatus(ModuleStatus status) {
    if (currentStatus != status) {
        currentStatus = status;
    }
}

void ModuleState::setProgress(uint8_t progressPercent) {
    progress = min(progressPercent, (uint8_t)100);
}

void ModuleState::setSolved(bool solved) {
    if (this->solved != solved) {
        this->solved = solved;
        
        // Send MODULE_SOLVED message when solved
        if (solved) {
            uint8_t solvedData[1];
            solvedData[0] = MODULE_SOLVED;
            sendCanMessage(CAN_ID_TIMER, solvedData, 1);
            
            // Play solved sound
            uint8_t audioData[1];
            audioData[0] = AUDIO_DEFUSED;
            sendCanMessage(CAN_ID_AUDIO, audioData, 1);
            
            // Update status
            currentStatus = MODULE_STATUS_SOLVED;
        }
    }
}

// ============================================================================
// STRIKE HANDLING
// ============================================================================

void ModuleState::triggerStrike() {
    // Send strike message to timer
    uint8_t strikeData[1];
    strikeData[0] = MODULE_STRIKE;
    sendCanMessage(CAN_ID_TIMER, strikeData, 1);
    
    // Play strike sound
    uint8_t audioData[1];
    audioData[0] = AUDIO_STRIKE;
    sendCanMessage(CAN_ID_AUDIO, audioData, 1);
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

