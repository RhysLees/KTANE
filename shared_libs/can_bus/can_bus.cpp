#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h"
#include "can_bus.h"

#define CAN_SPI_PIN 17
#define CAN_INT_PIN 20

volatile bool canInterruptFlag = false;
volatile uint32_t canInterruptCount = 0;
uint16_t thisModuleId = 0xFFFF; // Default uninitialized - prevents accidental message acceptance
bool canBusInitialized = false;

// ID negotiation variables
bool idConflictDetected = false;
uint8_t currentModuleType = 0;
uint8_t currentInstanceId = 0;

MCP_CAN CAN(CAN_SPI_PIN);

#define MAX_CAN_CALLBACKS 8
static CanMessageCallback canCallbacks[MAX_CAN_CALLBACKS];
static uint8_t callbackCount = 0;

void onCanInterrupt() {
  canInterruptFlag = true;
  canInterruptCount++;
  // Note: Keep this minimal - we're in an interrupt context
}

void initCanBus(uint16_t fullCanId) {
  thisModuleId = fullCanId;
  
  Serial.print("DEBUG: Initializing CAN bus with ID 0x");
  Serial.println(fullCanId, HEX);
  Serial.flush(); // Ensure debug message is printed

  uint8_t initResult = CAN.begin(MCP_NORMAL, CAN_500KBPS, MCP_8MHZ);
  Serial.print("DEBUG: CAN.begin() returned: ");
  Serial.println(initResult);
  Serial.flush();

  if (initResult == CAN_OK) {
    Serial.println("CAN init OK - using MCP_NORMAL directly");
    Serial.flush();
    
    pinMode(CAN_INT_PIN, INPUT);
    Serial.print("DEBUG: Setting up interrupt on pin ");
    Serial.println(CAN_INT_PIN);
    Serial.print("DEBUG: Pin state before interrupt setup: ");
    Serial.println(digitalRead(CAN_INT_PIN));
    Serial.flush();
    
    attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
    Serial.println("DEBUG: Interrupt attached successfully");
    
    // Test if INT pin is working by reading it again
    Serial.print("DEBUG: Pin state after interrupt setup: ");
    Serial.println(digitalRead(CAN_INT_PIN));
    Serial.flush();
    
    canBusInitialized = true;
    
    Serial.print("CAN module ID set to 0x");
    Serial.println(thisModuleId, HEX);
    Serial.println("DEBUG: CAN bus fully initialized and ready");
    Serial.flush();
  } else {
    Serial.print("CAN init FAIL - error code: ");
    Serial.println(initResult);
    Serial.println("DEBUG: Continuing without CAN bus");
    Serial.flush();
    canBusInitialized = false;
    // Don't halt - allow testing without CAN hardware
    return;
  }
}

void registerCanCallback(CanMessageCallback callback) {
  if (callbackCount < MAX_CAN_CALLBACKS) {
    canCallbacks[callbackCount++] = callback;
  }
}

void handleCanMessages() {
  if (!canBusInitialized) {
    return;
  }
  
  // Check if interrupt flag is set OR if INT pin is LOW (messages available)
  // This handles both interrupt-driven and polling scenarios
  if (!canInterruptFlag && digitalRead(CAN_INT_PIN) == HIGH) {
    // No interrupt flag and INT pin is HIGH = no messages available
    return;
  }
  
  // Clear interrupt flag at start of processing
  canInterruptFlag = false;
  
  // Process messages while available (but limit to prevent blocking)
  uint8_t messageCount = 0;
  const uint8_t MAX_MESSAGES_PER_CALL = 3; // Prevent blocking main loop
  
  while (messageCount < MAX_MESSAGES_PER_CALL) {
    // Check if messages are actually available
    if (CAN.checkReceive() != CAN_MSGAVAIL) {
      break; // No more messages
    }
    
    long unsigned int id;
    unsigned char len = 0;
    unsigned char buf[8];

    // Read the message
    if (CAN.readMsgBuf(&id, &len, buf) != CAN_OK) {
      Serial.println("DEBUG: Failed to read CAN message");
      break; // Error reading message
    }

    // Validate message length
    if (len > 8) {
      Serial.println("DEBUG: Invalid CAN message length");
      continue; // Skip invalid message
    }

    messageCount++;

    // Log ALL received messages (even filtered ones)
    Serial.print("DEBUG: Raw CAN message received - ID: 0x");
    Serial.print(id, HEX);
    Serial.print(", thisModuleId: 0x");
    Serial.println(thisModuleId, HEX);
    
    // Check for loopback (receiving our own messages)
    if (len >= 2) {
      uint8_t senderType = buf[0];
      uint8_t senderInstance = buf[1];
      uint16_t senderCanId = ((senderType & 0x3F) << 5) | (senderInstance & 0x1F);
      
      if (senderCanId == thisModuleId) {
        Serial.println("DEBUG: LOOPBACK DETECTED - Received our own message!");
        Serial.print("DEBUG: Our ID: 0x");
        Serial.print(thisModuleId, HEX);
        Serial.print(", Sender ID: 0x");
        Serial.println(senderCanId, HEX);
        continue; // Skip processing our own messages
      }
    }
    
    printCanMessage(id, buf, len, false);

    // Handle ID negotiation messages on global channels
    if (len >= 3 && (id & 0x1F) == 0x00) { // Global channel (instance 0x00)
      uint8_t msgType = buf[0];
      uint8_t moduleType = buf[1];
      uint8_t instanceId = buf[2];
      
      if (msgType == ID_PROBE && moduleType == currentModuleType) {
        // Someone is probing for an ID within our module type
        if (instanceId == currentInstanceId && currentInstanceId != 0) {
          // They're probing OUR ID! Tell them it's taken
          uint8_t takenData[3] = {ID_TAKEN, moduleType, instanceId};
          sendCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), takenData, 3);
          Serial.print("ID Negotiation: Told someone that ID ");
          Serial.print(instanceId);
          Serial.println(" is taken");
        }
      }
      else if (msgType == ID_TAKEN && moduleType == currentModuleType) {
        // Someone told us an ID is taken during our negotiation
        idConflictDetected = true;
      }
    }

    // Filter to this module or broadcast messages
    if (id != thisModuleId && id != CAN_ID_BROADCAST) {
      Serial.print("CAN FILTERED: Message for 0x");
      Serial.print(id, HEX);
      Serial.print(" (we are 0x");
      Serial.print(thisModuleId, HEX);
      Serial.println(")");
      continue; // Skip to next message
    }

    Serial.println("CAN ACCEPTED: Message passed filter, calling callbacks");
    for (uint8_t i = 0; i < callbackCount; i++) {
      if (canCallbacks[i]) {
        canCallbacks[i](id, buf, len);
      }
    }
  }
  
  // If we processed the maximum number of messages, there might be more
  if (messageCount >= MAX_MESSAGES_PER_CALL) {
    Serial.println("DEBUG: Processed max messages per call, more may be available");
  }
}

void sendCanMessage(uint16_t receiverID, const uint8_t* data, uint8_t dataLen) {
  if (!canBusInitialized) {
    Serial.println("DEBUG: sendCanMessage called but CAN bus not initialized");
    return;
  }
  
  // Build standardized message: [senderType, senderInstance, messageType, ...messageData]
  uint8_t fullMessage[8]; // CAN max is 8 bytes
  uint8_t senderType = (thisModuleId >> 5) & 0x7F;
  uint8_t senderInstance = thisModuleId & 0x1F;
  
  // First 3 bytes are always: sender type, sender instance, message type
  fullMessage[0] = senderType;
  fullMessage[1] = senderInstance;
  if (dataLen > 0) {
    fullMessage[2] = data[0]; // First byte of data is message type
    
    // Copy remaining message data (up to 4 more bytes)
    uint8_t maxDataBytes = min(dataLen - 1, 5);
    for (uint8_t i = 0; i < maxDataBytes; i++) {
      fullMessage[3 + i] = data[1 + i];
    }
    uint8_t totalLen = 3 + maxDataBytes;
    
    CAN.sendMsgBuf(receiverID, 0, totalLen, (byte*)fullMessage);
    printCanMessage(receiverID, fullMessage, totalLen, true);
  } else {
    // No message data, just send sender info
    CAN.sendMsgBuf(receiverID, 0, 2, (byte*)fullMessage);
    printCanMessage(receiverID, fullMessage, 2, true);
  }
}

const char* getMessageTypeName(uint8_t msgType) {
  switch (msgType) {
    case ID_PROBE: return "ID_PROBE";
    case ID_TAKEN: return "ID_TAKEN";
    case TIMER_GAME_START: return "TIMER_GAME_START";
    case TIMER_GAME_STOP: return "TIMER_GAME_STOP";
    case TIMER_STRIKE_UPDATE: return "TIMER_STRIKE_UPDATE";
    case TIMER_SERIAL_NUMBER: return "TIMER_SERIAL_NUMBER";
    case TIMER_RESET: return "TIMER_RESET";
    case TIMER_TIME_UPDATE: return "TIMER_TIME_UPDATE";
    case TIMER_COUNTDOWN: return "TIMER_COUNTDOWN";
    case MODULE_REGISTER: return "MODULE_REGISTER";
    case MODULE_STRIKE: return "MODULE_STRIKE";
    case MODULE_SOLVED: return "MODULE_SOLVED";
    case MODULE_STATUS: return "MODULE_STATUS";
    case MODULE_HEARTBEAT: return "MODULE_HEARTBEAT";
    case 0x30: return "AUDIO_*"; // Audio messages start at 0x30
    default: return "UNKNOWN";
  }
}

const char* getModuleTypeName(uint8_t moduleType) {
  switch (moduleType) {
    case CAN_TYPE_TIMER: return "TYPE_TIMER";
    case CAN_TYPE_AUDIO: return "TYPE_AUDIO";
    case CAN_TYPE_WIRES: return "TYPE_WIRES";
    case CAN_TYPE_BUTTON: return "TYPE_BUTTON";
    case CAN_TYPE_KEYPAD: return "TYPE_KEYPAD";
    case CAN_TYPE_SIMON: return "TYPE_SIMON";
    case CAN_TYPE_WHOS: return "TYPE_WHOS";
    case CAN_TYPE_MEMORY: return "TYPE_MEMORY";
    case CAN_TYPE_MORSE: return "TYPE_MORSE";
    case CAN_TYPE_COMPLICATED_WIRES: return "TYPE_COMPLICATED_WIRES";
    case CAN_TYPE_WIRE_SEQUENCES: return "TYPE_WIRE_SEQUENCES";
    case CAN_TYPE_MAZE: return "TYPE_MAZE";
    case CAN_TYPE_PASSWORD: return "TYPE_PASSWORD";
    case CAN_TYPE_VENTING_GAS: return "TYPE_VENTING_GAS";
    case CAN_TYPE_CAPACITOR_DISCHARGE: return "TYPE_CAPACITOR_DISCHARGE";
    case CAN_TYPE_KNOB: return "TYPE_KNOB";
    case CAN_TYPE_SERIAL_DISPLAY: return "TYPE_SERIAL_DISPLAY";
    case CAN_TYPE_INDICATOR_PANEL: return "TYPE_INDICATOR_PANEL";
    case CAN_TYPE_BATTERY_HOLDER: return "TYPE_BATTERY_HOLDER";
    case CAN_TYPE_PORT_PANEL: return "TYPE_PORT_PANEL";
    case CAN_TYPE_BROADCAST: return "TYPE_BROADCAST";
    default: return "UNKNOWN_TYPE";
  }
}

void printCanIdInfo(uint16_t canId, const char* prefix) {
  uint8_t moduleType = (canId >> 5) & 0x7F;
  uint8_t instanceId = canId & 0x1F;
  
  Serial.print(prefix);
  Serial.print("[");
  Serial.print(getModuleTypeName(moduleType));
  Serial.print(" - ");
  Serial.print(instanceId);
  Serial.print("]");
}

inline void printCanMessage(uint16_t id, const uint8_t* data, uint8_t len, bool sent) {
  // Direction
  if (sent) {
    Serial.print("CAN TX");
  } else {
    Serial.print("CAN RX");
  }
  
  // Show sender (this module) and receiver (target)
  if (sent) {
    printCanIdInfo(thisModuleId, " - S");
    printCanIdInfo(id, " - R");
  } else {
    printCanIdInfo(id, " - S");
    printCanIdInfo(thisModuleId, " - R");
  }
  
  // Message type only
  if (len > 0) {
    Serial.print(" - ");
    
    // Context-aware message type detection
    if (id == CAN_ID_AUDIO && len == 1) {
      // Audio messages - interpret as audio commands
      Serial.print("AUDIO_");
      switch (data[0]) {
        case 0x01: Serial.print("BEEP_NORMAL"); break;
        case 0x02: Serial.print("BEEP_FAST"); break;
        case 0x03: Serial.print("BEEP_HIGH"); break;
        case 0x04: Serial.print("STRIKE"); break;
        case 0x05: Serial.print("DEFUSED"); break;
        case 0x06: Serial.print("EXPLODED"); break;
        case 0x07: Serial.print("CORRECT_TIME"); break;
        case 0x08: Serial.print("GAME_OVER_FANFARE"); break;
        case 0x09: Serial.print("ALARM_CLOCK_BEEP"); break;
        case 0x0A: Serial.print("ALARM_CLOCK_SNOOZE"); break;
        case 0x0B: Serial.print("ALARM_EMERGENCY"); break;
        case 0x0C: Serial.print("SIMON_RED"); break;
        case 0x0D: Serial.print("SIMON_GREEN"); break;
        case 0x0E: Serial.print("SIMON_YELLOW"); break;
        case 0x0F: Serial.print("SIMON_BLUE"); break;
        default: Serial.print("UNKNOWN_0x"); Serial.print(data[0], HEX); break;
      }
    } else {
      // Regular protocol messages
      Serial.print(getMessageTypeName(data[0]));
    }
  }

  Serial.println();
}

// ============================================================================
// ID NEGOTIATION SYSTEM IMPLEMENTATION
// ============================================================================

bool negotiateInstanceId(uint8_t moduleType, uint8_t* assignedId) {
  if (!canBusInitialized) return false;
  
  currentModuleType = moduleType;
  currentInstanceId = 0;
  
  Serial.print("ID Negotiation: Starting negotiation for module type 0x");
  Serial.println(moduleType, HEX);
  
  // Try instance IDs from 0x01 to 0x1F
  for (uint8_t candidateId = 0x01; candidateId <= ID_MAX_INSTANCE; candidateId++) {
    Serial.print("ID Negotiation: Trying instance ID ");
    Serial.println(candidateId);
    
    // Multi-probe approach for better collision detection
    bool idAvailable = true;
    for (int probe = 0; probe < 3; probe++) {
      // Send probe message to global channel (direct send during negotiation)
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)probeData);
      printCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), probeData, 3, true);
      
      // Wait for responses with jitter (reduced timeout for faster startup)
      idConflictDetected = false;
      int probeDelay = 200 + random(0, 50); // Reduced from 500ms to 200ms + jitter
      unsigned long startTime = millis();
      while (millis() - startTime < probeDelay) {
        handleCanMessages(); // Process incoming messages
        if (idConflictDetected) {
          idAvailable = false;
          Serial.print("ID Negotiation: ID ");
          Serial.print(candidateId);
          Serial.print(" is taken (probe ");
          Serial.print(probe + 1);
          Serial.println(")");
          break;
        }
      }
      
      if (!idAvailable) break;
      
      // Small delay between probes (reduced for faster startup)
      delay(20 + random(0, 30));
    }
    
    if (idAvailable) {
      // Final verification probe before claiming
      Serial.print("ID Negotiation: Final verification for ID ");
      Serial.println(candidateId);
      
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)probeData);
      printCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), probeData, 3, true);
      
      idConflictDetected = false;
      unsigned long startTime = millis();
      while (millis() - startTime < 200) { // Reduced from ID_PROBE_TIMEOUT_MS (500ms) to 200ms
        handleCanMessages();
        if (idConflictDetected) {
          Serial.print("ID Negotiation: ID ");
          Serial.print(candidateId);
          Serial.println(" was taken during final verification");
          idAvailable = false;
          break;
        }
      }
      
      if (idAvailable) {
        // Success! Claim the ID
        currentInstanceId = candidateId;
        *assignedId = candidateId;
        
        Serial.print("ID Negotiation: Successfully claimed ID ");
        Serial.println(candidateId);
        return true;
      }
    }
    
    // Add exponential backoff if we've tried several IDs
    if (candidateId >= 3) {
      int backoffDelay = random(100, 500) * candidateId;
      Serial.print("ID Negotiation: Backing off for ");
      Serial.print(backoffDelay);
      Serial.println("ms");
      delay(backoffDelay);
    }
  }
  
  Serial.println("ID Negotiation: Failed - no available IDs");
  return false;
}

bool assignUniqueId(uint8_t moduleType) {
  if (!canBusInitialized) return false;
  
  Serial.print("ID Assignment: Starting for module type 0x");
  Serial.println(moduleType, HEX);
  
  // Random delay to prevent simultaneous negotiation (reduced for faster startup)
  int randomDelay = random(50, 500); // 50-500ms delay (reduced from 100-2000ms)
  Serial.print("ID Assignment: Waiting ");
  Serial.print(randomDelay);
  Serial.println("ms before negotiating...");
  delay(randomDelay);
  
  // Negotiate instance ID
  uint8_t assignedId = 0;
  if (negotiateInstanceId(moduleType, &assignedId)) {
    uint16_t finalCanId = CAN_INSTANCE_ID(moduleType, assignedId);
    updateCanId(finalCanId);
    Serial.print("ID Assignment: Successfully assigned instance ID ");
    Serial.print(assignedId);
    Serial.print(" (CAN ID 0x");
    Serial.print(finalCanId, HEX);
    Serial.println(")");
    return true;
  } else {
    Serial.println("ID Assignment: Failed to negotiate - using default instance ID 0x01");
    uint16_t defaultCanId = CAN_INSTANCE_ID(moduleType, 0x01);
    updateCanId(defaultCanId);
    return false;
  }
}

void updateCanId(uint16_t newCanId) {
  thisModuleId = newCanId;
  Serial.print("CAN ID updated to 0x");
  Serial.println(newCanId, HEX);
}

uint8_t getCurrentInstanceId() {
  return thisModuleId & 0x1F; // Extract instance ID from current CAN ID
}

uint16_t getCurrentModuleId() {
  return thisModuleId;
}

void sendHeartbeat(const uint8_t* data, uint8_t len) {
  if (!canBusInitialized || thisModuleId == 0xFFFF) {
    // CAN bus not initialized or no valid module ID
    return;
  }
  
  // Legacy function - use implementation directly since we have a pointer, not array
  sendCanMessage(CAN_ID_TIMER, data, len);
}

void printCanBusStatus() {
  Serial.println("=== CAN BUS STATUS ===");
  Serial.print("CAN Bus Initialized: ");
  Serial.println(canBusInitialized ? "YES" : "NO");
  Serial.print("Current Module ID: 0x");
  Serial.println(thisModuleId, HEX);
  Serial.print("Current Instance ID: ");
  Serial.println(getCurrentInstanceId());
  Serial.print("Current Module Type: 0x");
  Serial.println(currentModuleType, HEX);
  Serial.print("Callback Count: ");
  Serial.println(callbackCount);
  Serial.println("======================");
}
