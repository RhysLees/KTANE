// CAN Debug Receive Module
// Uses the can_bus library to receive and display all messages

#include <Arduino.h>
#include <can_bus.h>

static unsigned long totalMessageCount = 0;

// Callback for regular CAN messages (filtered by module ID)
void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  totalMessageCount++;
  
  Serial.print("[MSG #");
  Serial.print(totalMessageCount);
  Serial.print("] ");
  Serial.print("[");
  Serial.print(millis());
  Serial.print("ms] ");
  
  // Print receiver ID (who this message is addressed to)
  Serial.print("RX_ID: 0x");
  if(id < 0x10) Serial.print("0");
  Serial.print(id, HEX);
  
  // Print sender ID (extracted from message)
  Serial.print(" | Sender: 0x");
  if(senderId < 0x10) Serial.print("0");
  Serial.print(senderId, HEX);
  
  // Print length
  Serial.print(" | Len: ");
  Serial.print(len);
  
  // Print data
  Serial.print(" | Data: ");
  for(uint8_t i = 0; i < len; i++) {
    Serial.print("0x");
    if(data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    if(i < len - 1) Serial.print(" ");
  }
  
  // Try to decode message type
  Serial.print(" | ");
  if(len > 0) {
    uint8_t msgType = data[0];
    const char* msgTypeName = getMessageTypeName(msgType);
    Serial.print(msgTypeName);
    
    // Show sender module type if available
    if(senderId != 0) {
      uint8_t senderType = (senderId >> 5) & 0x3F;
      Serial.print(" (from ");
      Serial.print(getModuleTypeName(senderType));
      Serial.print(")");
    }
  }
  
  Serial.println();
}

// Raw callback for ALL messages (before filtering)
void onRawCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp) {
  // This will be called for all messages, even if they're filtered out
  // Useful for debugging - you can see everything on the bus
}

void setup()
{
  Serial.begin(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  Serial.println("==========================================");
  Serial.println("CAN Debug Receive Module");
  Serial.println("Using can_bus library");
  Serial.println("==========================================");

  // Initialize CAN bus with DEBUGGER ID
  initCanBus(CAN_ID_DEBUGGER);
  
  Serial.println("CAN bus initialized");
  Serial.print("Module ID: 0x");
  Serial.println(CAN_ID_DEBUGGER, HEX);
  Serial.println();
  
  // Register callback for filtered messages
  registerCanCallback(onCanMessage);
  
  // Register raw callback to see ALL messages (optional, for debugging)
  registerRawCanCallback(onRawCanMessage);
  
  Serial.println("Listening for messages...");
  Serial.println("Messages addressed to DEBUGGER or BROADCAST will be displayed");
  Serial.println();
}

void loop()
{
  static unsigned long lastHeartbeat = 0;
  unsigned long now = millis();
  
  // Print heartbeat every 5 seconds
  if(now - lastHeartbeat >= 5000) {
    lastHeartbeat = now;
    Serial.print("[HEARTBEAT] Uptime: ");
    Serial.print(now / 1000);
    Serial.print("s | Messages received: ");
    Serial.print(totalMessageCount);
    Serial.print(" | Rate: ");
    Serial.print(totalMessageCount * 1000 / now);
    Serial.println(" msg/s");
    Serial.println();
  }
  
  // Process incoming CAN messages
  handleCanMessages();
  
  delay(1);
}

