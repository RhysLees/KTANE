#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <simon_says.h>
#include <module_state.h>

SimonSays simonSays;

bool initialization_complete = false;
uint8_t countdown_seconds = 0;

// Callbacks for module_state
void onGameStateChange(bool running) {
    if (initialization_complete || !running) {
        // Only start game if initialized, but always allow stopping
        simonSays.onGameStateChange(running);
        if (running) {
            Serial.println("Simon Says: Game started");
        } else {
            Serial.println("Simon Says: Game stopped");
        }
    } else {
        Serial.println("Waiting for initialization to complete...");
    }
}

void onStrike(uint8_t strikes) {
    // Strike count is now managed by module_state
    // No need to update Simon Says - it reads from module_state directly
    Serial.print("Simon Says: Strike count updated to ");
    Serial.println(strikes);
}

void onSerialNumber(const String& serial) {
    simonSays.setSerialNumber(serial);
}

void onDiscovered() {
    // Called when module is discovered by timer
    // Discovery state is now handled by module_state
    Serial.println("Simon Says: Module discovered by timer");
    
    // Mark as initialized when discovered (countdown may or may not be used)
    // If countdown is used, it will override this when it reaches 0
    if (!initialization_complete) {
        initialization_complete = true;
        simonSays.setInitializationComplete(true);
        Serial.println("Simon Says: Initialization complete (discovered)");
    }
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    // Let module_state handle timer messages first
    moduleStateHandleCanMessage(id, senderId, data, len);
    
    // Handle module-specific messages (TIMER_COUNTDOWN)
    if ((id == CAN_ID_TIMER || id == CAN_ID_BROADCAST) && len >= 1) {
        uint8_t msgType = data[0];
        
        if (msgType == TIMER_COUNTDOWN) {
            if (len >= 2) {
                countdown_seconds = data[1];
                
                if (countdown_seconds == 0) {
                    // Countdown reached 0 - mark as initialized
                    initialization_complete = true;
                    simonSays.setInitializationComplete(true);
                    Serial.println("Simon Says: Initialization complete (countdown finished)");
                    
                    // If game is already running, start it now
                    if (globalModuleState && globalModuleState->isGameRunning()) {
                        simonSays.onGameStateChange(true);
                    }
                }
            }
        }
    }
    
    // Note: TIMER_RESET is handled by module_state via callback (onGameStateChange -> stopGame -> reset)
    // No need to pass to simonSays.handleCanMessage() unless there are module-specific messages
}

void printStatus() {
    uint8_t instanceId = getCurrentInstanceId();
    uint16_t canId = CAN_INSTANCE_ID(CAN_TYPE_SIMON, instanceId);
    
    Serial.println("=== STATUS ===");
    Serial.print("Instance ID: ");
    Serial.println(instanceId);
    Serial.print("CAN ID: 0x");
    Serial.println(canId, HEX);
    Serial.print("Serial Number: ");
    if (globalModuleState) {
        Serial.println(globalModuleState->getSerialNumber());
    } else {
        Serial.println("N/A");
    }
    Serial.print("Strikes: ");
    if (globalModuleState) {
        Serial.println(globalModuleState->getStrikeCount());
    } else {
        Serial.println("N/A");
    }
    Serial.print("Game Running: ");
    if (globalModuleState) {
        Serial.println(globalModuleState->isGameRunning() ? "YES" : "NO");
    } else {
        Serial.println("N/A");
    }
    Serial.print("Discovered: ");
    if (globalModuleState) {
        Serial.println(globalModuleState->isDiscoveredByTimer() ? "YES" : "NO");
    } else {
        Serial.println("N/A");
    }
    Serial.print("Initialized: ");
    Serial.println(initialization_complete ? "YES" : "NO");
    if (countdown_seconds > 0) {
        Serial.print("Countdown: ");
        Serial.print(countdown_seconds);
        Serial.println(" seconds");
    }
    Serial.println("==============");
}

void printHelp() {
    Serial.println("Simon Says Commands:");
    Serial.println("  RESET        - Reset module");
    Serial.println("  SERIAL <xxx> - Set serial number");
    Serial.println("  STRIKES      - Show current strike count");
    Serial.println("  START        - Start game");
    Serial.println("  STOP         - Stop game");
    Serial.println("  STATUS       - Show module status");
    Serial.println("  HELP         - Show this help");
}

void handleSerialCommands() {
    if (!Serial.available()) return;
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    
    if (input == "RESET") {
        simonSays.reset();
        Serial.println("Module reset");
    } 
    else if (input.startsWith("SERIAL ")) {
        String newSerial = input.substring(7);
        if (newSerial.length() == 6) {
            simonSays.setSerialNumber(newSerial);
            Serial.print("Serial number set to ");
            Serial.println(newSerial);
        } else {
            Serial.println("Invalid serial number format (must be 6 characters)");
        }
    } 
    else if (input.startsWith("STRIKES ")) {
        String strikeStr = input.substring(8);
        uint8_t strikes = strikeStr.toInt();
        if (strikes <= 3) {
            Serial.print("Strike count: ");
            if (globalModuleState) {
                Serial.print(globalModuleState->getStrikeCount());
                Serial.print(" (managed by timer, cannot set manually)");
            } else {
                Serial.print("N/A");
            }
            Serial.println();
        } else {
            Serial.println("Invalid strike count (must be 0-3)");
        }
    } 
    else if (input == "START") {
        simonSays.onGameStateChange(true);
        Serial.println("Game started");
    } 
    else if (input == "STOP") {
        simonSays.onGameStateChange(false);
        Serial.println("Game stopped");
    } 
    else if (input == "HELP") {
        printHelp();
    } 
    else if (input == "STATUS") {
        printStatus();
    } 
    else {
        Serial.println("Unknown command. Type HELP for available commands.");
    }
}

void setup() {
    Serial.begin(115200); // USB Serial
    Serial1.begin(115200); // UART Serial
    delay(5000);
    randomSeed(millis());
    
    Serial1.println("===============================");
    Serial1.println("KTANE Simon Says Module v1.0");
    Serial1.println("===============================");
    
    // Initialize CAN bus and negotiate unique ID
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    registerCanCallback(onCanMessage);
    assignUniqueId(CAN_TYPE_SIMON);
    
    uint8_t instanceId = getCurrentInstanceId();
    uint16_t finalCanId = getCurrentModuleId();
    Serial.print("Instance ID: ");
    Serial.println(instanceId);
    Serial.print("CAN ID: 0x");
    Serial.println(finalCanId, HEX);
    
    // Initialize module_state with status LED on pin 11
    initModuleState(SIMON_STATUS_LED);
    
    // Set callbacks for game state, strikes, serial number, and discovery
    if (globalModuleState) {
        globalModuleState->setGameStateCallback(onGameStateChange);
        globalModuleState->setStrikeCallback(onStrike);
        globalModuleState->setSerialNumberCallback(onSerialNumber);
        globalModuleState->setDiscoveredCallback(onDiscovered);
    }
    
    // Initialize Simon Says module
    // Strike count is managed by module_state, no need to set it here
    simonSays.begin();
    
    Serial.println("Module initialized with module_state library");
    Serial.println("Type HELP for available commands");
    Serial.println("===============================");
}

void loop() {
    simonSays.update();
    handleCanMessages();
    handleSerialCommands();
    updateModuleState();
    
    // Update module state status based on Simon Says state
    if (globalModuleState) {
        ModuleStatus status = MODULE_STATUS_IDLE;
        switch (simonSays.getState()) {
            case SimonState::IDLE:
                status = MODULE_STATUS_IDLE;
                break;
            case SimonState::DISPLAYING:
            case SimonState::WAITING_INPUT:
                status = MODULE_STATUS_ACTIVE;
                break;
            case SimonState::SOLVED:
                status = MODULE_STATUS_SOLVED;
                break;
            default:
                status = MODULE_STATUS_ACTIVE;
                break;
        }
        setModuleStateStatus(status);
        setModuleStateProgress((simonSays.getSequenceLength() * 100) / 5); // 5 is max sequence length
        
        // Handle solved state - module_state will handle LED automatically
        bool wasSolved = globalModuleState->isSolved();
        bool isNowSolved = simonSays.isSolved();
        setModuleStateSolved(isNowSolved);
        
        // Check if we need to trigger a strike
        // When Simon Says enters STRIKE state, trigger it via module_state
        static SimonState lastState = SimonState::IDLE;
        if (simonSays.getState() == SimonState::STRIKE && lastState != SimonState::STRIKE) {
            // Module just entered strike state - trigger strike via module_state
            if (globalModuleState) {
                globalModuleState->triggerStrike();
            }
        }
        lastState = simonSays.getState();
    }
} 