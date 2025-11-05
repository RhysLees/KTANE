// CAN Debug Receive Module
// Uses the can_bus library to receive and display all messages
// Implements game state to track connected modules (sender)

#include <Arduino.h>
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
    
    Serial.println();
    Serial.print("*** NEW MODULE CONNECTED ***");
    Serial.print(" | ID: 0x");
    Serial.print(moduleId, HEX);
    Serial.print(" | Type: ");
    Serial.print(module->moduleTypeName);
    Serial.print(" | Type Code: 0x");
    Serial.println(module->moduleType, HEX);
    Serial.println();
    
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
      Serial.println();
      Serial.print("*** MODULE DISCONNECTED ***");
      Serial.print(" | ID: 0x");
      Serial.print(module->moduleId, HEX);
      Serial.print(" | Type: ");
      Serial.print(module->moduleTypeName);
      Serial.print(" | Last seen: ");
      Serial.print((now - module->lastMessageTime) / 1000);
      Serial.println(" seconds ago");
      Serial.println();
    }
    
    // Check if module reconnected
    if(!module->isConnected && (now - module->lastMessageTime <= MODULE_TIMEOUT_MS)) {
      module->isConnected = true;
      Serial.println();
      Serial.print("*** MODULE RECONNECTED ***");
      Serial.print(" | ID: 0x");
      Serial.print(module->moduleId, HEX);
      Serial.print(" | Type: ");
      Serial.println(module->moduleTypeName);
      Serial.println();
    }
  }
}

// Print connected modules status
void printGameState() {
  Serial.println("==========================================");
  Serial.println("GAME STATE - Connected Modules");
  Serial.println("==========================================");
  Serial.print("Total connected modules: ");
  Serial.println(connectedModuleCount);
  Serial.println();
  
  if(connectedModuleCount == 0) {
    Serial.println("No modules connected yet.");
    Serial.println();
    return;
  }
  
  for(uint8_t i = 0; i < connectedModuleCount; i++) {
    ConnectedModule* module = &connectedModules[i];
    unsigned long now = millis();
    unsigned long timeSinceLastMsg = now - module->lastMessageTime;
    
    Serial.print("Module ");
    Serial.print(i + 1);
    Serial.print(": ");
    Serial.print(module->moduleTypeName);
    Serial.print(" (ID: 0x");
    Serial.print(module->moduleId, HEX);
    Serial.print(")");
    Serial.print(" | Status: ");
    Serial.print(module->isConnected ? "CONNECTED" : "DISCONNECTED");
    Serial.print(" | Messages: ");
    Serial.print(module->messageCount);
    Serial.print(" | Last msg: ");
    Serial.print(timeSinceLastMsg / 1000);
    Serial.print("s ago");
    Serial.print(" | Uptime: ");
    Serial.print((now - module->firstSeen) / 1000);
    Serial.println("s");
  }
  
  Serial.println("==========================================");
  Serial.println();
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
  
  // Print sender ID (extracted from message) - decode it
  Serial.print(" | Sender: 0x");
  if(senderId < 0x10) Serial.print("0");
  Serial.print(senderId, HEX);
  if(senderId != 0) {
    uint8_t moduleType, instanceId;
    decodeCanId(senderId, &moduleType, &instanceId);
    Serial.print(" (");
    Serial.print(getModuleTypeName(moduleType));
    Serial.print(" #");
    Serial.print(instanceId);
    Serial.print(")");
  }
  
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
  static unsigned long lastGameStatePrint = 0;
  unsigned long now = millis();
  
  // Update module connections (check for timeouts)
  updateDebugModuleConnections();
  
  // Print heartbeat every 5 seconds
  if(now - lastHeartbeat >= 5000) {
    lastHeartbeat = now;
    Serial.print("[HEARTBEAT] Uptime: ");
    Serial.print(now / 1000);
    Serial.print("s | Messages received: ");
    Serial.print(totalMessageCount);
    Serial.print(" | Rate: ");
    if(now > 0) {
      Serial.print(totalMessageCount * 1000 / now);
    } else {
      Serial.print("0");
    }
    Serial.print(" msg/s");
    Serial.print(" | Connected modules: ");
    Serial.println(connectedModuleCount);
    Serial.println();
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

