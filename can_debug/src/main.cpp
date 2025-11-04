#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>

void onRawCanMessage(uint16_t receiverId, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp) {
    // Print timestamp
    Serial.print("[");
    Serial.print(timestamp);
    Serial.print("ms] ");
    
    // Print CAN ID (receiver ID) - who this message is addressed to
    Serial.print("RX_ID: 0x");
    if (receiverId < 0x10) Serial.print("0");
    Serial.print(receiverId, HEX);
    
    // Print sender ID (extracted from first 2 bytes if present)
    Serial.print(" | Sender: 0x");
    if (senderId < 0x10) Serial.print("0");
    Serial.print(senderId, HEX);
    
    // Print length
    Serial.print(" | Len: ");
    Serial.print(len);
    
    // Print raw data bytes
    Serial.print(" | Raw: ");
    for (uint8_t i = 0; i < len; i++) {
        Serial.print("0x");
        if (data[i] < 0x10) Serial.print("0");
        Serial.print(data[i], HEX);
        if (i < len - 1) Serial.print(" ");
    }
    
    // Try to decode message type
    Serial.print(" | ");
    
    if (len == 0) {
        Serial.print("EMPTY");
    } else {
        // Check for ID negotiation messages (they don't use sendCanMessage format)
        if (data[0] == ID_PROBE && len >= 3) {
            Serial.print("ID_PROBE");
            Serial.print(" (Type: 0x");
            Serial.print(data[1], HEX);
            Serial.print(", Instance: ");
            Serial.print(data[2]);
            Serial.print(")");
        } else if (data[0] == ID_TAKEN && len >= 3) {
            Serial.print("ID_TAKEN");
            Serial.print(" (Type: 0x");
            Serial.print(data[1], HEX);
            Serial.print(", Instance: ");
            Serial.print(data[2]);
            Serial.print(")");
        }
        // For messages sent via sendCanMessage(), sender ID is in first 2 bytes
        // so message type is at data[2]
        else if (len >= 3 && senderId > 0) {
            // Message likely sent via sendCanMessage(), so data[2] is message type
            uint8_t msgType = data[2];
            switch (msgType) {
                case TIMER_GAME_START:
                    Serial.print("TIMER_GAME_START");
                    break;
                case TIMER_GAME_STOP:
                    Serial.print("TIMER_GAME_STOP");
                    if (len >= 4) {
                        Serial.print(" (State: ");
                        Serial.print(data[3]);
                        Serial.print(")");
                    }
                    break;
                case TIMER_STRIKE_UPDATE:
                    Serial.print("TIMER_STRIKE_UPDATE");
                    if (len >= 4) {
                        Serial.print(" (Strikes: ");
                        Serial.print(data[3]);
                        Serial.print(")");
                    }
                    break;
                case TIMER_TIME_UPDATE:
                    Serial.print("TIMER_TIME_UPDATE");
                    if (len >= 7) {
                        unsigned long timeMs;
                        memcpy(&timeMs, &data[3], 4);
                        Serial.print(" (Time: ");
                        Serial.print(timeMs / 1000);
                        Serial.print("s)");
                    }
                    break;
                case TIMER_SERIAL_NUMBER:
                    Serial.print("TIMER_SERIAL_NUMBER");
                    break;
                case MODULE_REGISTER:
                    Serial.print("MODULE_REGISTER");
                    break;
                case MODULE_SOLVED:
                    Serial.print("MODULE_SOLVED");
                    break;
                case MODULE_STRIKE:
                    Serial.print("MODULE_STRIKE");
                    break;
                case MODULE_HEARTBEAT:
                    Serial.print("MODULE_HEARTBEAT");
                    break;
                case MODULE_PING:
                    Serial.print("MODULE_PING");
                    break;
                default:
                    // Check if it's an audio command
                    if (senderId == CAN_ID_AUDIO || receiverId == CAN_ID_AUDIO) {
                        Serial.print("AUDIO_CMD: 0x");
                        Serial.print(msgType, HEX);
                    } else {
                        Serial.print("UNKNOWN_MSG: 0x");
                        Serial.print(msgType, HEX);
                    }
                    break;
            }
        } else if (len == 1 && receiverId == CAN_ID_AUDIO) {
            // Audio commands might be single byte
            Serial.print("AUDIO_CMD: 0x");
            Serial.print(data[0], HEX);
        } else {
            Serial.print("UNKNOWN_FORMAT");
        }
    }
    
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(500);  // Give Serial time to stabilize
    
    Serial.println("==========================================");
    Serial.println("CAN Bus Debug Module");
    Serial.println("==========================================");
    Serial.println("Listening to ALL CAN messages on bus");
    Serial.println("Messages will be printed as received");
    Serial.print("Module ID: 0x");
    Serial.println(CAN_INSTANCE_ID(CAN_TYPE_DEBUGGER, 0x00), HEX);
    Serial.println("==========================================");
    
    Serial.println("Initializing CAN bus...");
    // Initialize with DEBUGGER ID
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_DEBUGGER, 0x00));
    
    // Register raw callback to receive ALL messages (before filtering)
    registerRawCanCallback(onRawCanMessage);
    
    Serial.println("CAN bus initialized and ready");
    Serial.println("Waiting for messages...");
    Serial.println();
}

void loop() {
    // Process all incoming CAN messages
    handleCanMessages();
    
    // Small delay to prevent overwhelming the serial output
    delay(1);
}
