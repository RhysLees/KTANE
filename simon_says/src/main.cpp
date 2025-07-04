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
    // Handle messages from the timer module
    if (id == CAN_ID_TIMER && len > 0) {
        uint8_t msgType = data[0];
        
        switch (msgType) {
            case 0x10: // GAME_START
                Serial.println("Simon Says: Game start signal received");
                gameRunning = true;
                simonSays.onGameStateChange(true);
                break;
                
            case 0x11: // GAME_STOP
                Serial.println("Simon Says: Game stop signal received");
                gameRunning = false;
                simonSays.onGameStateChange(false);
                break;
                
            case 0x12: // STRIKE_UPDATE
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
                
            case 0x13: // SERIAL_NUMBER
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
                
            case 0x14: // RESET
                Serial.println("Simon Says: Reset signal received");
                simonSays.reset();
                gameRunning = false;
                currentStrikes = 0;
                break;
                
            default:
                // Unknown message type
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
        Serial.println("  HELP         - Show this help");
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
    
    // Initialize CAN bus
    initCanBus(SIMON_CAN_ID);
    registerCanCallback(onCanMessage);
    
    // Initialize Simon Says module
    simonSays.begin();
    
    // Set default values
    serialNumber = "A1B2C3"; // Default serial number
    simonSays.setSerialNumber(serialNumber);
    simonSays.setStrikeCount(0);
    
    Serial.println("Simon Says: Module initialized");
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