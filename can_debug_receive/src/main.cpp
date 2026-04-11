// CAN Debug Receive Module
// Uses the can_bus library to receive and display all messages
// Implements game state to track connected modules (sender)

#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>

// Module connection tracking structure
struct ConnectedModule {
  uint16_t moduleId;
  uint8_t moduleType;
  String moduleTypeName;
  unsigned long firstSeen;
  unsigned long lastMessageTime;
  unsigned long messageCount;
  bool isConnected;
  unsigned long lastHeartbeat;
};

// Game state: track connected modules
#define MAX_CONNECTED_MODULES 8
static ConnectedModule connectedModules[MAX_CONNECTED_MODULES];
static uint8_t connectedModuleCount = 0;
static unsigned long totalMessageCount = 0;

// Module timeout (5 seconds)
#define MODULE_TIMEOUT_MS 5000

// Find or create module entry
ConnectedModule* findOrCreateModule(uint16_t moduleId) {
  // Check if module already exists
  for(uint8_t i = 0; i < connectedModuleCount; i++) {
    if(connectedModules[i].moduleId == moduleId) {
      return &connectedModules[i];
    }
  }
  
  // Create new module entry if space available
  if(connectedModuleCount < MAX_CONNECTED_MODULES) {
    ConnectedModule* module = &connectedModules[connectedModuleCount];
    module->moduleId = moduleId;
    module->moduleType = (moduleId >> 5) & 0x3F;
    module->moduleTypeName = getModuleTypeName(module->moduleType);
    module->firstSeen = millis();
    module->lastMessageTime = millis();
    module->messageCount = 0;
    module->isConnected = true;
    module->lastHeartbeat = millis();
    connectedModuleCount++;
    
    KTANE_CONSOLE_OUT.println();
    KTANE_CONSOLE_OUT.print("*** NEW MODULE CONNECTED ***");
    KTANE_CONSOLE_OUT.print(" | ID: 0x");
    KTANE_CONSOLE_OUT.print(moduleId, HEX);
    KTANE_CONSOLE_OUT.print(" | Type: ");
    KTANE_CONSOLE_OUT.print(module->moduleTypeName);
    KTANE_CONSOLE_OUT.print(" | Type Code: 0x");
    KTANE_CONSOLE_OUT.println(module->moduleType, HEX);
    KTANE_CONSOLE_OUT.println();
    
    return module;
  }
  
  return nullptr;
}

// Update module connection status (renamed to avoid conflict with can_bus library)
void updateDebugModuleConnections() {
  unsigned long now = millis();
  
  for(uint8_t i = 0; i < connectedModuleCount; i++) {
    ConnectedModule* module = &connectedModules[i];
    
    // Check for timeout
    if(module->isConnected && (now - module->lastMessageTime > MODULE_TIMEOUT_MS)) {
      module->isConnected = false;
      KTANE_CONSOLE_OUT.println();
      KTANE_CONSOLE_OUT.print("*** MODULE DISCONNECTED ***");
      KTANE_CONSOLE_OUT.print(" | ID: 0x");
      KTANE_CONSOLE_OUT.print(module->moduleId, HEX);
      KTANE_CONSOLE_OUT.print(" | Type: ");
      KTANE_CONSOLE_OUT.print(module->moduleTypeName);
      KTANE_CONSOLE_OUT.print(" | Last seen: ");
      KTANE_CONSOLE_OUT.print((now - module->lastMessageTime) / 1000);
      KTANE_CONSOLE_OUT.println(" seconds ago");
      KTANE_CONSOLE_OUT.println();
    }
    
    // Check if module reconnected
    if(!module->isConnected && (now - module->lastMessageTime <= MODULE_TIMEOUT_MS)) {
      module->isConnected = true;
      KTANE_CONSOLE_OUT.println();
      KTANE_CONSOLE_OUT.print("*** MODULE RECONNECTED ***");
      KTANE_CONSOLE_OUT.print(" | ID: 0x");
      KTANE_CONSOLE_OUT.print(module->moduleId, HEX);
      KTANE_CONSOLE_OUT.print(" | Type: ");
      KTANE_CONSOLE_OUT.println(module->moduleTypeName);
      KTANE_CONSOLE_OUT.println();
    }
  }
}

// Print connected modules status
void printGameState() {
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println("GAME STATE - Connected Modules");
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.print("Total connected modules: ");
  KTANE_CONSOLE_OUT.println(connectedModuleCount);
  KTANE_CONSOLE_OUT.println();
  
  if(connectedModuleCount == 0) {
    KTANE_CONSOLE_OUT.println("No modules connected yet.");
    KTANE_CONSOLE_OUT.println();
    return;
  }
  
  for(uint8_t i = 0; i < connectedModuleCount; i++) {
    ConnectedModule* module = &connectedModules[i];
    unsigned long now = millis();
    unsigned long timeSinceLastMsg = now - module->lastMessageTime;
    
    KTANE_CONSOLE_OUT.print("Module ");
    KTANE_CONSOLE_OUT.print(i + 1);
    KTANE_CONSOLE_OUT.print(": ");
    KTANE_CONSOLE_OUT.print(module->moduleTypeName);
    KTANE_CONSOLE_OUT.print(" (ID: 0x");
    KTANE_CONSOLE_OUT.print(module->moduleId, HEX);
    KTANE_CONSOLE_OUT.print(")");
    KTANE_CONSOLE_OUT.print(" | Status: ");
    KTANE_CONSOLE_OUT.print(module->isConnected ? "CONNECTED" : "DISCONNECTED");
    KTANE_CONSOLE_OUT.print(" | Messages: ");
    KTANE_CONSOLE_OUT.print(module->messageCount);
    KTANE_CONSOLE_OUT.print(" | Last msg: ");
    KTANE_CONSOLE_OUT.print(timeSinceLastMsg / 1000);
    KTANE_CONSOLE_OUT.print("s ago");
    KTANE_CONSOLE_OUT.print(" | Uptime: ");
    KTANE_CONSOLE_OUT.print((now - module->firstSeen) / 1000);
    KTANE_CONSOLE_OUT.println("s");
  }
  
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println();
}

// Callback for regular CAN messages (filtered by module ID)
void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  totalMessageCount++;
  
  // Update game state: track sender module
  if(senderId != 0) {
    ConnectedModule* module = findOrCreateModule(senderId);
    if(module) {
      module->lastMessageTime = millis();
      module->messageCount++;
      module->isConnected = true;
      
      // Update heartbeat if this is a heartbeat message
      if(len > 0 && data[0] == MODULE_HEARTBEAT) {
        module->lastHeartbeat = millis();
      }
    }
  }
  
  KTANE_CONSOLE_OUT.print("[MSG #");
  KTANE_CONSOLE_OUT.print(totalMessageCount);
  KTANE_CONSOLE_OUT.print("] ");
  KTANE_CONSOLE_OUT.print("[");
  KTANE_CONSOLE_OUT.print(millis());
  KTANE_CONSOLE_OUT.print("ms] ");
  
  // Print receiver ID (who this message is addressed to)
  KTANE_CONSOLE_OUT.print("RX_ID: 0x");
  if(id < 0x10) KTANE_CONSOLE_OUT.print("0");
  KTANE_CONSOLE_OUT.print(id, HEX);
  
  // Print sender ID (extracted from message) - decode it
  KTANE_CONSOLE_OUT.print(" | Sender: 0x");
  if(senderId < 0x10) KTANE_CONSOLE_OUT.print("0");
  KTANE_CONSOLE_OUT.print(senderId, HEX);
  if(senderId != 0) {
    uint8_t moduleType, instanceId;
    decodeCanId(senderId, &moduleType, &instanceId);
    KTANE_CONSOLE_OUT.print(" (");
    KTANE_CONSOLE_OUT.print(getModuleTypeName(moduleType));
    KTANE_CONSOLE_OUT.print(" #");
    KTANE_CONSOLE_OUT.print(instanceId);
    KTANE_CONSOLE_OUT.print(")");
  }
  
  // Print length
  KTANE_CONSOLE_OUT.print(" | Len: ");
  KTANE_CONSOLE_OUT.print(len);
  
  // Print data
  KTANE_CONSOLE_OUT.print(" | Data: ");
  for(uint8_t i = 0; i < len; i++) {
    KTANE_CONSOLE_OUT.print("0x");
    if(data[i] < 0x10) KTANE_CONSOLE_OUT.print("0");
    KTANE_CONSOLE_OUT.print(data[i], HEX);
    if(i < len - 1) KTANE_CONSOLE_OUT.print(" ");
  }
  
  // Try to decode message type
  KTANE_CONSOLE_OUT.print(" | ");
  if(len > 0) {
    uint8_t msgType = data[0];
    const char* msgTypeName = getMessageTypeName(msgType);
    KTANE_CONSOLE_OUT.print(msgTypeName);
    
    // Show sender module type if available
    if(senderId != 0) {
      uint8_t senderType = (senderId >> 5) & 0x3F;
      KTANE_CONSOLE_OUT.print(" (from ");
      KTANE_CONSOLE_OUT.print(getModuleTypeName(senderType));
      KTANE_CONSOLE_OUT.print(")");
    }
  }
  
  KTANE_CONSOLE_OUT.println();
}

// Raw callback for ALL messages (before filtering)
void onRawCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp) {
  // This will be called for all messages, even if they're filtered out
  // Useful for debugging - you can see everything on the bus
}

void setup()
{
  ktaneConsoleInit(115200);
  delay(2000);  // Give time for serial monitor to connect
  
  KTANE_CONSOLE_OUT.println("==========================================");
  KTANE_CONSOLE_OUT.println("CAN Debug Receive Module");
  KTANE_CONSOLE_OUT.println("Using can_bus library");
  KTANE_CONSOLE_OUT.println("==========================================");

  // Initialize CAN bus with DEBUGGER ID
  initCanBus(CAN_ID_DEBUGGER);
  
  KTANE_CONSOLE_OUT.println("CAN bus initialized");
  KTANE_CONSOLE_OUT.print("Module ID: 0x");
  KTANE_CONSOLE_OUT.println(CAN_ID_DEBUGGER, HEX);
  KTANE_CONSOLE_OUT.println();
  
  // Register callback for filtered messages
  registerCanCallback(onCanMessage);
  
  // Register raw callback to see ALL messages (optional, for debugging)
  registerRawCanCallback(onRawCanMessage);
  
  KTANE_CONSOLE_OUT.println("Listening for messages...");
  KTANE_CONSOLE_OUT.println("Messages addressed to DEBUGGER or BROADCAST will be displayed");
  KTANE_CONSOLE_OUT.println();
}

void loop()
{
  static unsigned long lastHeartbeat = 0;
  static unsigned long lastGameStatePrint = 0;
  unsigned long now = millis();
  
  // Update module connections (check for timeouts)
  updateDebugModuleConnections();
  
  // Print heartbeat every 5 seconds
  if(now - lastHeartbeat >= 5000) {
    lastHeartbeat = now;
    KTANE_CONSOLE_OUT.print("[HEARTBEAT] Uptime: ");
    KTANE_CONSOLE_OUT.print(now / 1000);
    KTANE_CONSOLE_OUT.print("s | Messages received: ");
    KTANE_CONSOLE_OUT.print(totalMessageCount);
    KTANE_CONSOLE_OUT.print(" | Rate: ");
    if(now > 0) {
      KTANE_CONSOLE_OUT.print(totalMessageCount * 1000 / now);
    } else {
      KTANE_CONSOLE_OUT.print("0");
    }
    KTANE_CONSOLE_OUT.print(" msg/s");
    KTANE_CONSOLE_OUT.print(" | Connected modules: ");
    KTANE_CONSOLE_OUT.println(connectedModuleCount);
    KTANE_CONSOLE_OUT.println();
  }
  
  // Print game state every 10 seconds
  if(now - lastGameStatePrint >= 10000) {
    lastGameStatePrint = now;
    printGameState();
  }
  
  // Process incoming CAN messages
  handleCanMessages();
  
  delay(1);
}

