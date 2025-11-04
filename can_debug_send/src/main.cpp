// CAN Debug Send Module
// Uses the can_bus library to send test messages

#include <Arduino.h>
#include <can_bus.h>

void setup()
{
  Serial.begin(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  Serial.println("==========================================");
  Serial.println("CAN Debug Send Module");
  Serial.println("Using can_bus library");
  Serial.println("==========================================");

  // Initialize CAN bus with DEBUGGER_SENDER ID
  initCanBus(CAN_ID_DEBUGGER_SENDER);
  
  Serial.println("CAN bus initialized");
  Serial.print("Module ID: 0x");
  Serial.println(CAN_ID_DEBUGGER_SENDER, HEX);
  Serial.println();
  Serial.println("Sending test messages every 1 second...");
  Serial.println("Messages will be sent to:");
  Serial.print("  - CAN_ID_DEBUGGER (0x");
  Serial.print(CAN_ID_DEBUGGER, HEX);
  Serial.println(")");
  Serial.print("  - CAN_ID_BROADCAST (0x");
  Serial.print(CAN_ID_BROADCAST, HEX);
  Serial.println(")");
  Serial.println();
}

void loop()
{
  static unsigned long lastSend = 0;
  static uint8_t messageNumber = 0;
  unsigned long now = millis();
  
  // Send messages every 1 second
  if(now - lastSend >= 1000) {
    lastSend = now;
    messageNumber++;
    
    Serial.print("=== Sending Message #");
    Serial.print(messageNumber);
    Serial.println(" ===");
    
    // Test 1: Send to DEBUGGER
    Serial.println("Test 1: Sending to CAN_ID_DEBUGGER");
    uint8_t test1[] = {0xAA, 0xBB, 0xCC, messageNumber};
    sendCanMessage(CAN_ID_DEBUGGER, test1, 4);
    Serial.print("  Sent: ");
    for(uint8_t i = 0; i < 4; i++) {
      Serial.print("0x");
      if(test1[i] < 0x10) Serial.print("0");
      Serial.print(test1[i], HEX);
      if(i < 3) Serial.print(" ");
    }
    Serial.println();
    delay(100);
    
    // Test 2: Send broadcast message
    Serial.println("Test 2: Sending to CAN_ID_BROADCAST");
    uint8_t test2[] = {0xFF, 0xEE, 0xDD, messageNumber};
    sendCanMessage(CAN_ID_BROADCAST, test2, 4);
    Serial.print("  Sent: ");
    for(uint8_t i = 0; i < 4; i++) {
      Serial.print("0x");
      if(test2[i] < 0x10) Serial.print("0");
      Serial.print(test2[i], HEX);
      if(i < 3) Serial.print(" ");
    }
    Serial.println();
    delay(100);
    
    // Test 3: Send standard message types
    Serial.println("Test 3: Sending MODULE_HEARTBEAT to TIMER");
    uint8_t test3[] = {MODULE_HEARTBEAT};
    sendCanMessage(CAN_ID_TIMER, test3, 1);
    Serial.println("  Sent: MODULE_HEARTBEAT");
    
    Serial.println();
  }
  
  // Process incoming CAN messages (in case we receive anything)
  handleCanMessages();
  
  delay(10);
}

