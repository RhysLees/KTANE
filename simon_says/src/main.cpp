#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <simon_says.h>

SimonSays simonSays;

bool gameRunning = false;
uint8_t currentStrikes = 0;
String serialNumber = "";
bool initialization_complete = false;
uint8_t countdown_seconds = 0;

void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    if ((id == CAN_ID_TIMER || id == CAN_ID_BROADCAST) && len >= 3) {
        uint8_t senderType = data[0];
        uint8_t senderInstance = data[1];
        uint8_t msgType = data[2];
        
        switch (msgType) {
            case TIMER_GAME_START:
                if (initialization_complete) {
                    gameRunning = true;
                    simonSays.onGameStateChange(true);
                } else {
                    Serial.println("Waiting for initialization to complete...");
                }
                break;
                
            case TIMER_GAME_STOP:
                gameRunning = false;
                simonSays.onGameStateChange(false);
                break;
                
            case TIMER_STRIKE_UPDATE:
                if (len >= 4) {
                    uint8_t strikes = data[3];
                    if (strikes != currentStrikes) {
                        currentStrikes = strikes;
                        simonSays.setStrikeCount(strikes);
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
    
    simonSays.handleCanMessage(id, data, len);
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
    Serial.println(serialNumber);
    Serial.print("Strikes: ");
    Serial.println(currentStrikes);
    Serial.print("Game Running: ");
    Serial.println(gameRunning ? "YES" : "NO");
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
    Serial.println("  STRIKES <n>  - Set strike count (0-3)");
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
            serialNumber = newSerial;
            simonSays.setSerialNumber(serialNumber);
            Serial.print("Serial number set to ");
            Serial.println(serialNumber);
        } else {
            Serial.println("Invalid serial number format (must be 6 characters)");
        }
    } 
    else if (input.startsWith("STRIKES ")) {
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
    } 
    else if (input == "START") {
        gameRunning = true;
        simonSays.onGameStateChange(true);
        Serial.println("Game started");
    } 
    else if (input == "STOP") {
        gameRunning = false;
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
    Serial.begin(115200);
    delay(50);
    randomSeed(millis());
    
    Serial.println("===============================");
    Serial.println("KTANE Simon Says Module v1.0");
    Serial.println("===============================");
    
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
    
    // Register with timer module
    uint8_t registerData[1] = {MODULE_REGISTER};
    sendCanMessage(CAN_ID_TIMER, registerData, 1);
    Serial.println("Registered with timer module");
    
    // Initialize with empty values - will be received from timer
    serialNumber = "";
    simonSays.setStrikeCount(0);
    simonSays.begin();
    
    Serial.println("Module initialized and ready!");
    Serial.println("Type HELP for available commands");
    Serial.println("===============================");
}

void loop() {
    simonSays.update();
    handleCanMessages();
    handleSerialCommands();
} 