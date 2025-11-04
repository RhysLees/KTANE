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
    Serial.print("[LOG] sendRawCanMessage() called - Sender: 0x");
    Serial.print(senderId, HEX);
    Serial.print(", Receiver: 0x");
    Serial.print(receiverId, HEX);
    Serial.print(", DataLen: ");
    Serial.println(dataLen);
    
    if (!canBusInitialized) {
        Serial.println("[ERROR] CAN bus not initialized, cannot send message");
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
        
        Serial.print("[LOG] Prepared message data: ");
        for (uint8_t i = 0; i < dataLen + 2; i++) {
            Serial.print("0x");
            if (messageData[i] < 0x10) Serial.print("0");
            Serial.print(messageData[i], HEX);
            if (i < dataLen + 1) Serial.print(" ");
        }
        Serial.println();
        
        // Send with prepended sender ID
        Serial.println("[LOG] Calling CAN.sendMsgBuf()...");
        byte result = CAN.sendMsgBuf(receiverId, 0, dataLen + 2, (byte*)messageData);
        
        if (result == CAN_OK) {
            Serial.print("[LOG] Message sent successfully: Sender=0x");
            Serial.print(senderId, HEX);
            Serial.print(" -> Receiver=0x");
            Serial.print(receiverId, HEX);
            Serial.print(", Len=");
            Serial.println(dataLen + 2);
        } else {
            Serial.print("[ERROR] CAN.sendMsgBuf() failed with result: ");
            Serial.println(result);
        }
    } else {
        Serial.print("[ERROR] Invalid dataLen: ");
        Serial.print(dataLen);
        Serial.println(" (must be 1-6)");
    }
}

void setup() {
    Serial.println("[LOG] Starting CAN Bus Debug Sender Module setup...");
    
    Serial.begin(115200);
    delay(5000);  // Give time to connect to serial monitor
    Serial.println("[LOG] Serial initialized at 115200 baud");
    
    Serial.println("==========================================");
    Serial.println("CAN Bus Debug Sender Module");
    Serial.println("==========================================");
    Serial.println("This module sends test messages from");
    Serial.println("various CAN IDs to test the CAN bus");
    Serial.println("==========================================");
    
    Serial.println("[LOG] Initializing CAN bus...");
    Serial.print("[LOG] CAN_SPI_PIN: ");
    Serial.println(CAN_SPI_PIN);
    Serial.print("[LOG] CAN_INT_PIN: ");
    Serial.println(CAN_INT_PIN);
    
    // Initialize SPI before CAN controller
    Serial.println("[LOG] Initializing SPI...");
    SPI.begin();
    Serial.println("[LOG] SPI initialized");
    
    // Initialize CAN bus
    uint8_t retries = 0;
    const uint8_t MAX_RETRIES = 5;
    
    while (retries < MAX_RETRIES) {
        Serial.print("[LOG] CAN.begin() attempt ");
        Serial.print(retries + 1);
        Serial.print("/");
        Serial.print(MAX_RETRIES);
        Serial.println("...");
        
        byte result = CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ);
        if (result == CAN_OK) {
            Serial.println("[LOG] CAN.begin() succeeded");
            
            Serial.println("[LOG] Configuring CAN masks and filters...");
            // Disable all CAN filters to receive all messages
            CAN.init_Mask(0, 0, 0x00000000);
            CAN.init_Mask(1, 0, 0x00000000);
            Serial.println("[LOG] CAN masks initialized");
            
            for (int i = 0; i < 6; i++) {
                CAN.init_Filt(i, 0, 0x00000000);
            }
            Serial.println("[LOG] CAN filters initialized");

            Serial.println("[LOG] Enabling OneShotTX mode...");
            CAN.enOneShotTX();
            
            Serial.println("[LOG] Setting CAN mode to NORMAL...");
            CAN.setMode(MCP_NORMAL);
            Serial.println("[LOG] CAN mode set to NORMAL");
            
            Serial.print("[LOG] Configuring interrupt pin ");
            Serial.print(CAN_INT_PIN);
            Serial.println("...");
            pinMode(CAN_INT_PIN, INPUT);
            
            Serial.println("[LOG] Attaching interrupt handler...");
            attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
            Serial.println("[LOG] Interrupt handler attached");
            
            canBusInitialized = true;
            Serial.println("[LOG] canBusInitialized set to true");
            
            Serial.println("[LOG] CAN bus initialization complete");
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
            Serial.print("[ERROR] CAN.begin() failed (attempt ");
            Serial.print(retries);
            Serial.print("/");
            Serial.print(MAX_RETRIES);
            Serial.print("), result: ");
            Serial.println(result);
            delay(1000);
        }
    }
    
    // If we get here, all retries failed
    Serial.println("[ERROR] CAN init FAILED after all retries");
}

void loop() {
    static unsigned long lastHeartbeat = 0;
    unsigned long now = millis();
    
    // Print heartbeat every second
    if (now - lastHeartbeat >= 1000) {
        lastHeartbeat = now;
        Serial.print("[HEARTBEAT] Debug sender module running - Uptime: ");
        Serial.print(now / 1000);
        Serial.println("s");
    }
    
    // Wait for serial input to trigger test messages
    if (Serial.available()) {
        Serial.println("[LOG] Serial input detected, clearing buffer...");
        while (Serial.available()) {
            Serial.read(); // Clear buffer
        }
        Serial.println("[LOG] Buffer cleared");
        
        Serial.println();
        Serial.println("=== Sending Test Messages ===");
        
        // Test 1: Send from TIMER ID to AUDIO
        Serial.println("[LOG] Starting Test 1...");
        Serial.println("Test 1: TIMER -> AUDIO (GAME_START)");
        uint8_t test1[] = {TIMER_GAME_START};
        sendRawCanMessage(CAN_ID_TIMER, CAN_ID_AUDIO, test1, 1);
        Serial.println("[LOG] Test 1 complete, delaying 100ms...");
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
        sendRawCanMessage(CAN_ID_DEBUGGER_SENDER, CAN_ID_DEBUGGER, test4, 1);
        delay(100);
        
        // Test 5: Send TIME_UPDATE with data
        Serial.println("Test 5: TIMER -> BROADCAST (TIME_UPDATE)");
        unsigned long timeMs = 60000; // 60 seconds
        uint8_t test5[5] = {TIMER_TIME_UPDATE};
        memcpy(&test5[1], &timeMs, 4);
        sendRawCanMessage(CAN_ID_DEBUGGER_SENDER, CAN_ID_BROADCAST, test5, 5);
        delay(100);
        
        // Test 6: Send ID_PROBE message (special format, no sender ID prepended)
        Serial.println("[LOG] Starting Test 6...");
        Serial.println("Test 6: ID_PROBE (raw format)");
        uint8_t test6[] = {ID_PROBE, CAN_ID_DEBUGGER_SENDER, 0x01};
        Serial.print("[LOG] Calling CAN.sendMsgBuf() for ID_PROBE to ID: 0x");
        Serial.println(CAN_INSTANCE_ID(CAN_ID_DEBUGGER_SENDER, 0x00), HEX);
        byte result = CAN.sendMsgBuf(CAN_INSTANCE_ID(CAN_ID_DEBUGGER_SENDER, 0x00), 0, 3, (byte*)test6);
        if (result == CAN_OK) {
            Serial.println("[LOG] ID_PROBE sent successfully");
            Serial.println("Sent: ID_PROBE (Type: SIMON, Instance: 1)");
        } else {
            Serial.print("[ERROR] ID_PROBE send failed with result: ");
            Serial.println(result);
        }
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

