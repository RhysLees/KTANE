// CAN Debug Send Module
// Uses the can_bus library to send registration and heartbeat messages as a SIMON module

#include <Arduino.h>
#include <can_bus.h>

// Track registration state
bool isRegistered = false;

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  if (len < 1) {
    return;
  }
  
  uint8_t msgType = data[0];
  
  // Check for TIMER_MODULE_DISCOVERED acknowledgment
  if (id == CAN_ID_TIMER && msgType == TIMER_MODULE_DISCOVERED) {
    if (!isRegistered) {
      isRegistered = true;
      Serial.println("==========================================");
      Serial.println("Registered with timer!");
      Serial.println("Switching to heartbeat mode...");
      Serial.println("==========================================");
    }
  }
}

void setup()
{
  Serial.begin(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  Serial.println("==========================================");
  Serial.println("CAN Debug Send Module");
  Serial.println("Using can_bus library - SIMON type");
  Serial.println("==========================================");

  // Initialize CAN bus with SIMON type and negotiate unique ID
  initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
  assignUniqueId(CAN_TYPE_SIMON);
  
  // Register CAN message callback
  registerCanCallback(onCanMessage);
  
  uint8_t instanceId = getCurrentInstanceId();
  uint16_t finalCanId = getCurrentModuleId();
  
  Serial.println("CAN bus initialized");
  Serial.print("Module Type: SIMON");
  Serial.print(" | Instance ID: ");
  Serial.print(instanceId);
  Serial.print(" | CAN ID: 0x");
  Serial.println(finalCanId, HEX);
  Serial.println();
  Serial.println("Sending MODULE_REGISTER every 1 second until timer responds...");
  Serial.println();
}

void loop()
{
  static unsigned long lastSend = 0;
  unsigned long now = millis();
  
  // Send MODULE_REGISTER every 1 second until registered
  // Once registered, send MODULE_HEARTBEAT every 1 second
  if(now - lastSend >= 1000) {
    lastSend = now;
    
    if (!isRegistered) {
      // Registration phase - send MODULE_REGISTER
      Serial.print("[");
      Serial.print(now / 1000);
      Serial.print("s] Sending MODULE_REGISTER to TIMER");
      
      uint8_t registerData[] = {MODULE_REGISTER};
      sendCanMessage(CAN_ID_TIMER, registerData, 1);
      
      Serial.println(" - Sent");
    } else {
      // Registered - send MODULE_HEARTBEAT
      Serial.print("[");
      Serial.print(now / 1000);
      Serial.print("s] Sending MODULE_HEARTBEAT to TIMER");
      
      uint8_t heartbeatData[] = {MODULE_HEARTBEAT};
      sendCanMessage(CAN_ID_TIMER, heartbeatData, 1);
      
      Serial.println(" - Sent");
    }
  }
  
  // Process incoming CAN messages
  handleCanMessages();
  
  delay(10);
}

