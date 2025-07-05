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

// CAN message callback
void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle messages from the timer module (direct or broadcast)
    if ((id == CAN_ID_TIMER || id == CAN_ID_BROADCAST) && len > 0) {
        uint8_t msgType = data[0];
        
        switch (msgType) {
            case TIMER_GAME_START:
                Serial.println("Simon Says: Game start signal received");
                gameRunning = true;
                simonSays.onGameStateChange(true);
                break;
                
            case TIMER_GAME_STOP:
                Serial.println("Simon Says: Game stop signal received");
                gameRunning = false;
                simonSays.onGameStateChange(false);
                break;
                
            case TIMER_STRIKE_UPDATE:
                if (len >= 2) {
                    uint8_t strikes = data[1];
                    if (strikes != currentStrikes) {
                        currentStrikes = strikes;
                        simonSays.setStrikeCount(strikes);
                        Serial.print("Simon Says: Strike count updated to ");
                        Serial.println(strikes);
                    }
                }
                break;
                
            case TIMER_SERIAL_NUMBER:
                if (len >= 7) {
                    char serial[7];
                    memcpy(serial, &data[1], 6);
                    serial[6] = '\0';
                    serialNumber = String(serial);
                    simonSays.setSerialNumber(serialNumber);
                    Serial.print("Simon Says: Serial number updated to ");
                    Serial.println(serialNumber);
                }
                break;
                
            case TIMER_RESET:
                Serial.println("Simon Says: Reset signal received");
                simonSays.reset();
                gameRunning = false;
                currentStrikes = 0;
                break;
                
            case TIMER_TIME_UPDATE:
                if (len >= 5) {
                    uint32_t timeMs = 0;
                    memcpy(&timeMs, &data[1], 4);
                    // Could update a time display or handle time-based logic
                    Serial.print("Simon Says: Time remaining: ");
                    Serial.print(timeMs / 1000);
                    Serial.println(" seconds");
                }
                break;
                
            default:
                Serial.print("Simon Says: Unknown timer message type: 0x");
                Serial.println(msgType, HEX);
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
    
    if (input == "STATUS") {
        simonSays.printStatus();
    } else if (input == "SEQUENCE") {
        simonSays.printSequence();
    } else if (input == "RULES") {
        simonSays.printRules();
    } else if (input == "RESET") {
        simonSays.reset();
        Serial.println("Simon Says: Module reset");
    } else if (input.startsWith("SERIAL ")) {
        String newSerial = input.substring(7);
        if (newSerial.length() == 6) {
            serialNumber = newSerial;
            simonSays.setSerialNumber(serialNumber);
            Serial.print("Simon Says: Serial number set to ");
            Serial.println(serialNumber);
        } else {
            Serial.println("Simon Says: Invalid serial number format (must be 6 characters)");
        }
    } else if (input.startsWith("STRIKES ")) {
        String strikeStr = input.substring(8);
        uint8_t strikes = strikeStr.toInt();
        if (strikes <= 3) {
            currentStrikes = strikes;
            simonSays.setStrikeCount(strikes);
            Serial.print("Simon Says: Strike count set to ");
            Serial.println(strikes);
        } else {
            Serial.println("Simon Says: Invalid strike count (must be 0-3)");
        }
    } else if (input == "START") {
        gameRunning = true;
        simonSays.onGameStateChange(true);
        Serial.println("Simon Says: Game started");
    } else if (input == "STOP") {
        gameRunning = false;
        simonSays.onGameStateChange(false);
        Serial.println("Simon Says: Game stopped");
    } else if (input == "HELP") {
        Serial.println("Simon Says Commands:");
        Serial.println("  STATUS       - Show module status");
        Serial.println("  SEQUENCE     - Show current sequence");
        Serial.println("  RULES        - Show color mapping rules");
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
        Serial.println("==================");
    } else {
        Serial.println("Simon Says: Unknown command. Type HELP for available commands.");
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
    uint8_t registerData[3];
    registerData[0] = MODULE_REGISTER;
    registerData[1] = CAN_TYPE_SIMON;
    registerData[2] = instanceId;
    sendCanMessage(CAN_ID_TIMER, registerData, 3);
    Serial.println("Simon Says: Registered with timer module");
    
    // Set default values
    serialNumber = "A1B2C3"; // Default serial number
    simonSays.setSerialNumber(serialNumber);
    simonSays.setStrikeCount(0);
    
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