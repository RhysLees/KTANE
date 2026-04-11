// CAN Debug Send Module
// Uses the can_bus library to send registration and heartbeat messages as a SIMON module

#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>

// Track registration state
bool isRegistered = false;

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  if (len < 1) {
    return;
  }
  
  uint8_t msgType = data[0];
  
  // Check for TIMER_MODULE_DISCOVERED acknowledgment
  // Timer sends this message directly to the module's ID, not CAN_ID_TIMER
  if (msgType == TIMER_MODULE_DISCOVERED) {
    if (!isRegistered) {
      isRegistered = true;
      KTANE_CONSOLE_OUT.println("==========================================");
      KTANE_CONSOLE_OUT.println("Registered with timer!");
      KTANE_CONSOLE_OUT.println("Switching to heartbeat mode...");
      KTANE_CONSOLE_OUT.println("==========================================");
    }
  }
}

void setup()
{
  ktaneConsoleInit(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println("CAN Debug Send Module");
  KTANE_CONSOLE_OUT.println("Using can_bus library - SIMON type");
  KTANE_CONSOLE_OUT.println("==========================================");

  // Initialize CAN bus with SIMON type and negotiate unique ID
  initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
  assignUniqueId(CAN_TYPE_SIMON);
  
  // Register CAN message callback
  registerCanCallback(onCanMessage);
  
  uint8_t instanceId = getCurrentInstanceId();
  uint16_t finalCanId = getCurrentModuleId();
  
  KTANE_CONSOLE_OUT.println("CAN bus initialized");
  KTANE_CONSOLE_OUT.print("Module Type: SIMON");
  KTANE_CONSOLE_OUT.print(" | Instance ID: ");
  KTANE_CONSOLE_OUT.print(instanceId);
  KTANE_CONSOLE_OUT.print(" | CAN ID: 0x");
  KTANE_CONSOLE_OUT.println(finalCanId, HEX);
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println("Sending MODULE_REGISTER every 1 second until timer responds...");
  KTANE_CONSOLE_OUT.println();
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
      KTANE_CONSOLE_OUT.print("[");
      KTANE_CONSOLE_OUT.print(now / 1000);
      KTANE_CONSOLE_OUT.print("s] Sending MODULE_REGISTER to TIMER");
      
      uint8_t registerData[] = {MODULE_REGISTER};
      sendCanMessage(CAN_ID_TIMER, registerData, 1);
      
      KTANE_CONSOLE_OUT.println(" - Sent");
    } else {
      // Registered - send MODULE_HEARTBEAT
      KTANE_CONSOLE_OUT.print("[");
      KTANE_CONSOLE_OUT.print(now / 1000);
      KTANE_CONSOLE_OUT.print("s] Sending MODULE_HEARTBEAT to TIMER");
      
      uint8_t heartbeatData[] = {MODULE_HEARTBEAT};
      sendCanMessage(CAN_ID_TIMER, heartbeatData, 1);
      
      KTANE_CONSOLE_OUT.println(" - Sent");
    }
  }
  
  // Process incoming CAN messages
  handleCanMessages();
  
  delay(10);
}

