#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <simon_says.h>

// Global Simon Says instance
SimonSays simonSays;

// Game state variables
bool gameRunning = false;
uint8_t currentStrikes = 0;
String serialNumber = "";
bool initialization_complete = false;
uint8_t countdown_seconds = 0;

// CAN message callback
void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle messages from the timer module (direct or broadcast)
    if ((id == CAN_ID_TIMER || id == CAN_ID_BROADCAST) && len >= 3) {
        // New message format: [senderType, senderInstance, messageType, ...messageData]
        uint8_t senderType = data[0];
        uint8_t senderInstance = data[1];
        uint8_t msgType = data[2];
        
        switch (msgType) {
            case TIMER_GAME_START:
                Serial.println("Simon Says: Game start signal received");
                if (initialization_complete) {
                    gameRunning = true;
                    simonSays.onGameStateChange(true);
                    Serial.println("Simon Says: Game started - module is now interactive!");
                } else {
                    Serial.println("Simon Says: Waiting for initialization to complete...");
                }
                break;
                
            case TIMER_GAME_STOP:
                Serial.println("Simon Says: Game stop signal received");
                gameRunning = false;
                simonSays.onGameStateChange(false);
                break;
                
            case TIMER_STRIKE_UPDATE:
                if (len >= 4) {
                    uint8_t strikes = data[3];
                    if (strikes != currentStrikes) {
                        currentStrikes = strikes;
                        simonSays.setStrikeCount(strikes);
                        Serial.print("Simon Says: Strike count updated to ");
                        Serial.println(strikes);
                    }
                }
                break;
                
            case TIMER_SERIAL_NUMBER:
                if (len >= 9) {
                    char serial[7];
                    memcpy(serial, &data[3], 6);
                    serial[6] = '\0';
                    serialNumber = String(serial);
                    simonSays.setSerialNumber(serialNumber);
                }
                break;
                
            case TIMER_RESET:
                simonSays.reset();
                gameRunning = false;
                currentStrikes = 0;
                break;
                
            case TIMER_TIME_UPDATE:
                if (len >= 7) {
                    uint32_t timeMs = 0;
                    memcpy(&timeMs, &data[3], 4);
                    // Could update a time display or handle time-based logic
                }
                break;
                
            case TIMER_COUNTDOWN:
                if (len >= 4) {
                    countdown_seconds = data[3];
                    
                    if (countdown_seconds == 0) {
                        initialization_complete = true;
                        simonSays.setInitializationComplete(true);
                    }
                }
                break;
                
            default:
                break;
        }
    }
    
    // Forward message to Simon Says handler
    simonSays.handleCanMessage(id, data, len);
}

// Serial command handler
void handleSerialCommands() {
    if (!Serial.available()) return;
    
    String input = Serial.readStringUntil('\n');
    input.trim();
    input.toUpperCase();
    
    if (input == "RESET") {
        simonSays.reset();
        Serial.println("Module reset");
    } else if (input.startsWith("SERIAL ")) {
        String newSerial = input.substring(7);
        if (newSerial.length() == 6) {
            serialNumber = newSerial;
            simonSays.setSerialNumber(serialNumber);
            Serial.print("Serial number set to ");
            Serial.println(serialNumber);
        } else {
            Serial.println("Invalid serial number format (must be 6 characters)");
        }
    } else if (input.startsWith("STRIKES ")) {
        String strikeStr = input.substring(8);
        uint8_t strikes = strikeStr.toInt();
        if (strikes <= 3) {
            currentStrikes = strikes;
            simonSays.setStrikeCount(strikes);
            Serial.print("Strike count set to ");
            Serial.println(strikes);
        } else {
            Serial.println("Invalid strike count (must be 0-3)");
        }
    } else if (input == "START") {
        gameRunning = true;
        simonSays.onGameStateChange(true);
        Serial.println("Game started");
    } else if (input == "STOP") {
        gameRunning = false;
        simonSays.onGameStateChange(false);
        Serial.println("Game stopped");
    } else if (input == "HELP") {
        Serial.println("Simon Says Commands:");
        Serial.println("  RESET        - Reset module");
        Serial.println("  SERIAL <xxx> - Set serial number");
        Serial.println("  STRIKES <n>  - Set strike count (0-3)");
        Serial.println("  START        - Start game");
        Serial.println("  STOP         - Stop game");
        Serial.println("  CAN_STATUS   - Show CAN ID and communication status");
        Serial.println("  HELP         - Show this help");
    } else if (input == "CAN_STATUS") {
        uint8_t instanceId = getCurrentInstanceId();
        uint16_t canId = CAN_INSTANCE_ID(CAN_TYPE_SIMON, instanceId);
        
        Serial.println("=== CAN STATUS ===");
        Serial.print("Module Type: 0x");
        Serial.println(CAN_TYPE_SIMON, HEX);
        Serial.print("Instance ID: ");
        Serial.println(instanceId);
        Serial.print("CAN ID: 0x");
        Serial.println(canId, HEX);
        Serial.print("Serial Number: ");
        Serial.println(serialNumber);
        Serial.print("Current Strikes: ");
        Serial.println(currentStrikes);
        Serial.print("Game Running: ");
        Serial.println(gameRunning ? "YES" : "NO");
        Serial.print("Initialization Complete: ");
        Serial.println(initialization_complete ? "YES" : "NO");
        if (countdown_seconds > 0) {
            Serial.print("Countdown: ");
            Serial.print(countdown_seconds);
            Serial.println(" seconds");
        }
        Serial.println("==================");
    } else {
        Serial.println("Unknown command. Type HELP for available commands.");
    }
}



void setup() {
    // Initialize serial communication
    Serial.begin(115200);
    delay(50); // slight delay for entropy
	randomSeed(millis());
    
    Serial.println("===============================");
    Serial.println("KTANE Simon Says Module v1.0");
    Serial.println("===============================");
    
    // Initialize CAN bus with global/temporary ID
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    registerCanCallback(onCanMessage);
    
    // Assign unique instance ID for this module type
    assignUniqueId(CAN_TYPE_SIMON);
    
    // Show final assignment
    uint8_t instanceId = getCurrentInstanceId();
    Serial.print("Simon Says: Final instance ID is ");
    Serial.println(instanceId);
    
    // Register with timer module
    uint8_t registerData[1];
    registerData[0] = MODULE_REGISTER;
                         sendCanMessage(CAN_ID_TIMER, registerData, 1);
    Serial.println("Simon Says: Registered with timer module");
    
    // Initialize with empty values - will be received from timer
    serialNumber = "";
    simonSays.setStrikeCount(0);
    
    // Don't set serial number yet - wait for it from timer
    Serial.println("Simon Says: Waiting for serial number from timer module...");
    
    // Initialize Simon Says module
    simonSays.begin();
    
    Serial.println("Simon Says: Module initialized and ready!");
    Serial.println("Type HELP for available commands");
    Serial.println("===============================");
}

void loop() {
    // Update Simon Says module (essential for game logic)
    simonSays.update();
    
    // Handle CAN messages
    handleCanMessages();
    
    // Handle serial commands
    handleSerialCommands();
} 