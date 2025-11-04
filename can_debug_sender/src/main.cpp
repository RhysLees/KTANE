#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h"
#include "can_bus.h"

#define CAN_SPI_PIN 17
#define CAN_INT_PIN 20

MCP_CAN CAN(CAN_SPI_PIN);

bool canBusInitialized = false;

void onCanInterrupt() {
    // Interrupt handler - can be empty for sender
}

void sendRawCanMessage(uint16_t senderId, uint16_t receiverId, const uint8_t* data, uint8_t dataLen) {
    if (!canBusInitialized) {
        return;
    }
    
    // Prepare message with sender ID prepended (matching can_bus library format)
    uint8_t messageData[8];
    
    if (dataLen > 0 && dataLen <= 6) { // Leave room for 2-byte sender ID
        // Add sender module ID (2 bytes)
        messageData[0] = (senderId >> 8) & 0xFF;  // High byte
        messageData[1] = senderId & 0xFF;         // Low byte
        
        // Add original data
        memcpy(&messageData[2], data, dataLen);
        
        // Send with prepended sender ID
        CAN.sendMsgBuf(receiverId, 0, dataLen + 2, (byte*)messageData);
        
        Serial.print("Sent: Sender=0x");
        Serial.print(senderId, HEX);
        Serial.print(" -> Receiver=0x");
        Serial.print(receiverId, HEX);
        Serial.print(", Len=");
        Serial.println(dataLen + 2);
    }
}

void setup() {
    Serial.begin(115200);
    delay(500);  // Give Serial time to stabilize
    
    Serial.println("==========================================");
    Serial.println("CAN Bus Debug Sender Module");
    Serial.println("==========================================");
    Serial.println("This module sends test messages from");
    Serial.println("various CAN IDs to test the CAN bus");
    Serial.println("==========================================");
    
    Serial.println("Initializing CAN bus...");
    
    // Initialize CAN bus
    uint8_t retries = 0;
    const uint8_t MAX_RETRIES = 5;
    
    while (retries < MAX_RETRIES) {
        if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
            // Disable all CAN filters to receive all messages
            CAN.init_Mask(0, 0, 0x00000000);
            CAN.init_Mask(1, 0, 0x00000000);
            
            for (int i = 0; i < 6; i++) {
                CAN.init_Filt(i, 0, 0x00000000);
            }

            CAN.enOneShotTX();
            CAN.setMode(MCP_NORMAL);
            
            pinMode(CAN_INT_PIN, INPUT_PULLUP);
            
            attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
            
            canBusInitialized = true;
            
            Serial.println("CAN bus initialized successfully");
            Serial.print("Module ID: 0x");
            Serial.println(CAN_ID_DEBUGGER_SENDER, HEX);
            Serial.println();
            Serial.println("Starting test message sequence...");
            Serial.println("Press Enter to send test messages");
            Serial.println();
            return;
        } else {
            retries++;
            Serial.print("CAN init failed (attempt ");
            Serial.print(retries);
            Serial.print("/");
            Serial.print(MAX_RETRIES);
            Serial.println(")");
            delay(1000);
        }
    }
    
    // If we get here, all retries failed
    Serial.println("CAN init FAILED after all retries");
}

void loop() {
    // Wait for serial input to trigger test messages
    if (Serial.available()) {
        while (Serial.available()) {
            Serial.read(); // Clear buffer
        }
        
        Serial.println();
        Serial.println("=== Sending Test Messages ===");
        
        // Test 1: Send from TIMER ID to AUDIO
        Serial.println("Test 1: TIMER -> AUDIO (GAME_START)");
        uint8_t test1[] = {TIMER_GAME_START};
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_AUDIO, test1, 1);
        delay(100);
        
        // Test 2: Send from TIMER ID to BROADCAST
        Serial.println("Test 2: TIMER -> BROADCAST (STRIKE_UPDATE)");
        uint8_t test2[] = {TIMER_STRIKE_UPDATE, 2};
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_BROADCAST, test2, 2);
        delay(100);
        
        // Test 3: Send from AUDIO ID to TIMER
        Serial.println("Test 3: AUDIO -> TIMER (HEARTBEAT)");
        uint8_t test3[] = {MODULE_HEARTBEAT};
        sendRawCanMessage(CAN_ID_AUDIO, CAN_ID_TIMER, test3, 1);
        delay(100);
        
        // Test 4: Send from SIMON ID to TIMER
        Serial.println("Test 4: SIMON -> TIMER (SOLVED)");
        uint8_t test4[] = {MODULE_SOLVED};
        uint16_t simonId = CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x01);
        sendRawCanMessage(simonId, CAN_ID_TIMER, test4, 1);
        delay(100);
        
        // Test 5: Send TIME_UPDATE with data
        Serial.println("Test 5: TIMER -> BROADCAST (TIME_UPDATE)");
        unsigned long timeMs = 60000; // 60 seconds
        uint8_t test5[5] = {TIMER_TIME_UPDATE};
        memcpy(&test5[1], &timeMs, 4);
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_BROADCAST, test5, 5);
        delay(100);
        
        // Test 6: Send ID_PROBE message (special format, no sender ID prepended)
        Serial.println("Test 6: ID_PROBE (raw format)");
        uint8_t test6[] = {ID_PROBE, CAN_TYPE_SIMON, 0x01};
        CAN.sendMsgBuf(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00), 0, 3, (byte*)test6);
        Serial.println("Sent: ID_PROBE (Type: SIMON, Instance: 1)");
        delay(100);
        
        // Test 7: Send from DEBUGGER_SENDER to DEBUGGER
        Serial.println("Test 7: DEBUGGER_SENDER -> DEBUGGER");
        uint8_t test7[] = {0xAA, 0xBB, 0xCC};
        sendRawCanMessage(CAN_ID_DEBUGGER_SENDER, CAN_ID_DEBUGGER, test7, 3);
        delay(100);
        
        // Test 8: Send to multiple different IDs
        Serial.println("Test 8: Multiple IDs");
        uint8_t test8[] = {0xFF};
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_AUDIO, test8, 1);
        delay(50);
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_SERIAL_DISPLAY, test8, 1);
        delay(50);
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_BROADCAST, test8, 1);
        delay(100);
        
        Serial.println();
        Serial.println("=== Test Messages Complete ===");
        Serial.println("Press Enter to send again");
    }
    
    delay(10);
}

