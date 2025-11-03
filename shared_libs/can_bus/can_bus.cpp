#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h"
#include "can_bus.h"

#define CAN_SPI_PIN 17
#define CAN_INT_PIN 20

volatile bool canInterruptFlag = false;
volatile uint32_t canInterruptCount = 0;
uint16_t thisModuleId = 0xFFFF;
bool canBusInitialized = false;
unsigned long lastMessageTime = 0;

// Connection detection system
bool audioModuleConnected = false;
bool serialDisplayConnected = false;
unsigned long lastAudioPing = 0;
unsigned long lastSerialDisplayPing = 0;
#define MODULE_TIMEOUT_MS 5000

// ID negotiation variables
bool idConflictDetected = false;
uint8_t currentModuleType = 0;
uint8_t currentInstanceId = 0;

MCP_CAN CAN(CAN_SPI_PIN);

#define MAX_CAN_CALLBACKS 8
static CanMessageCallback canCallbacks[MAX_CAN_CALLBACKS];
static uint8_t callbackCount = 0;

#define MAX_RAW_CAN_CALLBACKS 2
static RawCanMessageCallback rawCanCallbacks[MAX_RAW_CAN_CALLBACKS];
static uint8_t rawCallbackCount = 0;

void onCanInterrupt() {
  canInterruptFlag = true;
  canInterruptCount++;
}

void initCanBus(uint16_t fullCanId) {
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
      
      pinMode(CAN_INT_PIN, INPUT);
      
      // Check interrupt pin state
      bool intPinState = digitalRead(CAN_INT_PIN);
      if (Serial) {
        Serial.print("CAN INT pin initial state: ");
        Serial.println(intPinState ? "HIGH" : "LOW");
      }
      
      attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
      
      canBusInitialized = true;
      thisModuleId = fullCanId;
      
      if (Serial) {
        Serial.print("CAN ID: 0x");
        Serial.println(thisModuleId, HEX);
      }
      
      return;  // Success
    } else {
      retries++;
      if (Serial) {
        Serial.print("CAN init failed (attempt ");
        Serial.print(retries);
        Serial.print("/");
        Serial.print(MAX_RETRIES);
        Serial.println(")");
      }
      delay(1000);
    }
  }
  
  // If we get here, all retries failed
  if (Serial) {
    Serial.println("CAN init FAILED after all retries - continuing anyway");
  }
  canBusInitialized = false;
  thisModuleId = fullCanId;
}

void registerCanCallback(CanMessageCallback callback) {
  if (callbackCount < MAX_CAN_CALLBACKS) {
    canCallbacks[callbackCount++] = callback;
  }
}

void registerRawCanCallback(RawCanMessageCallback callback) {
  if (rawCallbackCount < MAX_RAW_CAN_CALLBACKS) {
    rawCanCallbacks[rawCallbackCount++] = callback;
  }
}

void handleIdNegotiation(uint16_t id, const uint8_t* buf, uint8_t len) {
  if (len >= 3 && (id & 0x1F) == 0x00 && id != CAN_ID_BROADCAST) {
    uint8_t msgType = buf[0];
    uint8_t moduleType = buf[1];
    uint8_t instanceId = buf[2];
    
    if (msgType == ID_PROBE && moduleType == currentModuleType) {
      if (instanceId == currentInstanceId && currentInstanceId != 0) {
        uint8_t takenData[3] = {ID_TAKEN, moduleType, instanceId};
        CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)takenData);
      }
    }
    else if (msgType == ID_TAKEN && moduleType == currentModuleType) {
      idConflictDetected = true;
    }
  }
}

void handleCanMessages() {
  // Process ALL available messages in the queue
  // Some CAN controllers queue multiple messages, so we need to process them all
  uint8_t messagesProcessed = 0;
  const uint8_t MAX_MESSAGES_PER_CALL = 10; // Limit to prevent blocking
  
  // ALWAYS poll for messages - interrupts are unreliable on some MCP2515 boards
  // checkReceive() is the source of truth for message availability
  byte status = CAN.checkReceive();
  
  // If interrupt flag is set, also acknowledge it
  if (canInterruptFlag) {
    canInterruptFlag = false;
  }
  
  // Minimal debug output (reduced to prevent serial buffer issues)
  static unsigned long lastPollDebug = 0;
  unsigned long now = millis();
  if (now - lastPollDebug >= 10000) {  // Every 10 seconds (very infrequent)
    lastPollDebug = now;
    if (Serial) {  // Only print if serial is available
      byte currentStatus = CAN.checkReceive();
      Serial.print("CAN: ");
      Serial.print(currentStatus == CAN_MSGAVAIL ? "RX" : "---");
      Serial.print(", INT=");
      Serial.print(digitalRead(CAN_INT_PIN) == LOW ? "L" : "H");
      if (lastMessageTime > 0) {
        Serial.print(", last=");
        Serial.print((now - lastMessageTime) / 1000);
        Serial.print("s");
      }
      Serial.println();
    }
  }
  
  while (messagesProcessed < MAX_MESSAGES_PER_CALL) {
    bool messageAvailable = false;
    
    // ALWAYS check checkReceive() first - it's the most reliable
    status = CAN.checkReceive();
    if (status == CAN_MSGAVAIL) {
      messageAvailable = true;
    }
    
    // Interrupt flag is secondary - sometimes doesn't fire
    if (canInterruptFlag) {
      canInterruptFlag = false;
      if (!messageAvailable) {
        // If interrupt fired but checkReceive says no message, that's odd
        Serial.println("CAN: Interrupt fired but checkReceive() says no message - investigating");
        messageAvailable = (CAN.checkReceive() == CAN_MSGAVAIL); // Double-check
      }
    }
    
    // If no message available, exit the loop
    if (!messageAvailable) {
      break;
    }
    
    messagesProcessed++;
    
    // Read the message
    long unsigned int id;
    unsigned char len = 0;
    unsigned char buf[8];

    CAN.readMsgBuf(&id, &len, buf);
    
    // Update last message time for diagnostics
    lastMessageTime = millis();

    // Handle ID negotiation messages first
    handleIdNegotiation(id, buf, len);

    // Update connection status - check direct messages from known modules
    if (id == CAN_ID_AUDIO) {
      if (!audioModuleConnected) {
        Serial.println("Audio module connected");
      }
      audioModuleConnected = true;
      lastAudioPing = millis();
    } else if (id == CAN_ID_SERIAL_DISPLAY) {
      if (!serialDisplayConnected) {
        Serial.println("Serial display connected");
      }
      serialDisplayConnected = true;
      lastSerialDisplayPing = millis();
    }

    // Call raw callbacks first (before filtering) - for logging/monitoring
    uint16_t senderId = 0;
    if (len >= 2) {
      senderId = (buf[0] << 8) | buf[1];
    }
    
    // Debug: Log all received messages
    Serial.print("CAN RX: ID=0x");
    Serial.print(id, HEX);
    Serial.print(", Sender=0x");
    Serial.print(senderId, HEX);
    Serial.print(", Len=");
    Serial.print(len);
    Serial.print(", Data=");
    for (uint8_t i = 0; i < len && i < 8; i++) {
      Serial.print("0x");
      Serial.print(buf[i], HEX);
      Serial.print(" ");
    }
    Serial.print(", thisModuleId=0x");
    Serial.println(thisModuleId, HEX);
    
    for (uint8_t i = 0; i < rawCallbackCount; i++) {
      if (rawCanCallbacks[i]) {
        rawCanCallbacks[i](id, senderId, buf, len, millis());
      }
    }

    // Filter to this module or broadcast messages only
    if (id != thisModuleId && id != CAN_ID_BROADCAST) {
      Serial.print("CAN: Message filtered out - ID 0x");
      Serial.print(id, HEX);
      Serial.print(" not for this module (0x");
      Serial.print(thisModuleId, HEX);
      Serial.println(") or broadcast");
      continue; // Continue to next message in queue
    }
    
    Serial.println("CAN: Message passed filter - calling callbacks");

    // Extract sender ID from first 2 bytes and shift data
    uint8_t shiftedData[8];
    uint8_t shiftedLen = len;
    
    if (len >= 2) {
      // Shift data to remove sender ID (copy bytes 2+ to start)
      shiftedLen = len - 2;
      memcpy(shiftedData, &buf[2], shiftedLen);
    } else {
      // Not enough data for sender ID, pass original data
      memcpy(shiftedData, buf, len);
    }

    // Call registered callbacks with extracted sender ID and shifted data
    for (uint8_t i = 0; i < callbackCount; i++) {
      if (canCallbacks[i]) {
        canCallbacks[i](id, senderId, shiftedData, shiftedLen);
      }
    }
    
    // Continue loop to check for more messages
  }
  
  // Log if we processed multiple messages
  if (messagesProcessed > 1) {
    Serial.print("CAN: Processed ");
    Serial.print(messagesProcessed);
    Serial.println(" messages in this call");
  }
}

void sendCanMessage(uint16_t receiverID, const uint8_t* data, uint8_t dataLen) {
  if (!canBusInitialized) {
    return;
  }
  
  // Check module connectivity for fixed modules
  if (receiverID == CAN_ID_AUDIO && !audioModuleConnected) {
    return;
  }
  if (receiverID == CAN_ID_SERIAL_DISPLAY && !serialDisplayConnected) {
    return;
  }
  
  // Automatically prepend sender module ID to message data
  if (dataLen > 0 && dataLen <= 6) { // Leave room for 2-byte sender ID
    uint8_t messageData[8];
    
    // Add sender module ID (2 bytes)
    messageData[0] = (thisModuleId >> 8) & 0xFF;  // High byte
    messageData[1] = thisModuleId & 0xFF;         // Low byte
    
    // Add original data
    memcpy(&messageData[2], data, dataLen);
    
    // Send with prepended sender ID
    CAN.sendMsgBuf(receiverID, 0, dataLen + 2, (byte*)messageData);
  }
}

const char* getMessageTypeName(uint8_t msgType) {
  switch (msgType) {
    case ID_PROBE: return "ID_PROBE";
    case ID_TAKEN: return "ID_TAKEN";
    case TIMER_GAME_START: return "GAME_START";
    case TIMER_GAME_STOP: return "GAME_STOP";
    case TIMER_STRIKE_UPDATE: return "STRIKE_UPDATE";
    case TIMER_SERIAL_NUMBER: return "SERIAL_NUMBER";
    case TIMER_RESET: return "RESET";
    case TIMER_TIME_UPDATE: return "TIME_UPDATE";
    case TIMER_COUNTDOWN: return "COUNTDOWN";
    case TIMER_MODULE_DISCOVERED: return "MODULE_DISCOVERED";
    case MODULE_REGISTER: return "REGISTER";
    case MODULE_STRIKE: return "STRIKE";
    case MODULE_SOLVED: return "SOLVED";
    case MODULE_STATUS: return "STATUS";
    case MODULE_HEARTBEAT: return "HEARTBEAT";
    case MODULE_PING: return "PING";
    case 0x30: return "AUDIO_*";
    default: return "UNKNOWN";
  }
}

const char* getModuleTypeName(uint8_t moduleType) {
  switch (moduleType) {
    case CAN_TYPE_TIMER: return "TIMER";
    case CAN_TYPE_AUDIO: return "AUDIO";
    case CAN_TYPE_SIMON: return "SIMON";
    case CAN_TYPE_SERIAL_DISPLAY: return "SERIAL_DISPLAY";
    case CAN_TYPE_BROADCAST: return "BROADCAST";
    default: return "UNKNOWN";
  }
}

bool negotiateInstanceId(uint8_t moduleType, uint8_t* assignedId) {
  if (thisModuleId == CAN_ID_TIMER) {
    Serial.println("Timer module cannot negotiate IDs");
    return false;
  }
  
  if (!canBusInitialized) return false;
  
  currentModuleType = moduleType;
  currentInstanceId = 0;
  
  for (uint8_t candidateId = 0x01; candidateId <= ID_MAX_INSTANCE; candidateId++) {
    bool idAvailable = true;
    
    // Probe for ID availability
    for (int probe = 0; probe < 3; probe++) {
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)probeData);
      
      idConflictDetected = false;
      int probeDelay = 200 + random(0, 50);
      unsigned long startTime = millis();
      
      while (millis() - startTime < probeDelay) {
        handleCanMessages();
        if (idConflictDetected) {
          idAvailable = false;
          break;
        }
      }
      
      if (!idAvailable) break;
      delay(20 + random(0, 30));
    }
    
    if (idAvailable) {
      // Final confirmation probe
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)probeData);
      
      idConflictDetected = false;
      unsigned long startTime = millis();
      while (millis() - startTime < 200) {
        handleCanMessages();
        if (idConflictDetected) {
          idAvailable = false;
          break;
        }
      }
      
      if (idAvailable) {
        currentInstanceId = candidateId;
        *assignedId = candidateId;
        return true;
      }
    }
    
    // Exponential backoff for higher IDs
    if (candidateId >= 3) {
      delay(random(100, 500) * candidateId);
    }
  }
  
  return false;
}

bool assignUniqueId(uint8_t moduleType) {
  if (thisModuleId == CAN_ID_TIMER) {
    Serial.println("Timer module cannot negotiate IDs");
    return false;
  }
  
  if (!canBusInitialized) return false;
  
  // Random startup delay to avoid collisions
  delay(random(50, 500));
  
  uint8_t assignedId = 0;
  if (negotiateInstanceId(moduleType, &assignedId)) {
    uint16_t finalCanId = CAN_INSTANCE_ID(moduleType, assignedId);
    updateCanId(finalCanId);
    Serial.print("Assigned ID: ");
    Serial.println(assignedId);
    return true;
  } else {
    // Fallback to default ID
    uint16_t defaultCanId = CAN_INSTANCE_ID(moduleType, 0x01);
    updateCanId(defaultCanId);
    Serial.println("Using default ID: 1");
    return false;
  }
}

void updateCanId(uint16_t newCanId) {
  thisModuleId = newCanId;
}

uint8_t getCurrentInstanceId() {
  return thisModuleId & 0x1F;
}

uint16_t getCurrentModuleId() {
  return thisModuleId;
}

void updateModuleConnections() {
  unsigned long now = millis();
  
  if (audioModuleConnected && (now - lastAudioPing > MODULE_TIMEOUT_MS)) {
    audioModuleConnected = false;
    Serial.println("Audio module disconnected");
  }
  
  if (serialDisplayConnected && (now - lastSerialDisplayPing > MODULE_TIMEOUT_MS)) {
    serialDisplayConnected = false;
    Serial.println("Serial display disconnected");
  }
}