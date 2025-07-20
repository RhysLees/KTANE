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

void onCanInterrupt() {
  canInterruptFlag = true;
  canInterruptCount++;
}

void initCanBus(uint16_t fullCanId) {
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    // Disable all CAN filters to receive all messages
    CAN.init_Mask(0, 0, 0x00000000);
    CAN.init_Mask(1, 0, 0x00000000);
    
    for (int i = 0; i < 6; i++) {
      CAN.init_Filt(i, 0, 0x00000000);
    }

    CAN.enOneShotTX();
    CAN.setMode(MCP_NORMAL);
    
    Serial.println("CAN bus initialized");
  } else {
    Serial.println("CAN init failed - retrying");
    delay(1000);
    initCanBus(fullCanId);
  }

  pinMode(CAN_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
  
  canBusInitialized = true;
  thisModuleId = fullCanId;
  
  Serial.print("CAN ID: 0x");
  Serial.println(thisModuleId, HEX);
}

void registerCanCallback(CanMessageCallback callback) {
  if (callbackCount < MAX_CAN_CALLBACKS) {
    canCallbacks[callbackCount++] = callback;
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
  if (!canInterruptFlag) return;
  canInterruptFlag = false;

  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    long unsigned int id;
    unsigned char len = 0;
    unsigned char buf[8];

    CAN.readMsgBuf(&id, &len, buf);

    // Handle ID negotiation messages first
    handleIdNegotiation(id, buf, len);

    // Update connection status
    if (id == CAN_ID_AUDIO) {
      audioModuleConnected = true;
      lastAudioPing = millis();
    } else if (id == CAN_ID_SERIAL_DISPLAY) {
      serialDisplayConnected = true;
      lastSerialDisplayPing = millis();
    }

    // Filter to this module or broadcast messages only
    if (id != thisModuleId && id != CAN_ID_BROADCAST) {
      return;
    }

    // Call registered callbacks
    for (uint8_t i = 0; i < callbackCount; i++) {
      if (canCallbacks[i]) {
        canCallbacks[i](id, buf, len);
      }
    }
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
  
  // Build standardized message: [senderType, senderInstance, messageType, ...messageData]
  uint8_t fullMessage[8];
  uint8_t senderType = (thisModuleId >> 5) & 0x7F;
  uint8_t senderInstance = thisModuleId & 0x1F;
  
  fullMessage[0] = senderType;
  fullMessage[1] = senderInstance;
  
  if (dataLen > 0) {
    fullMessage[2] = data[0]; // First byte is message type
    
    uint8_t maxDataBytes = min(dataLen - 1, 5);
    for (uint8_t i = 0; i < maxDataBytes; i++) {
      fullMessage[3 + i] = data[1 + i];
    }
    uint8_t totalLen = 3 + maxDataBytes;
    
    CAN.sendMsgBuf(receiverID, 0, totalLen, (byte*)fullMessage);
  } else {
    CAN.sendMsgBuf(receiverID, 0, 2, (byte*)fullMessage);
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
    case MODULE_REGISTER: return "REGISTER";
    case MODULE_STRIKE: return "STRIKE";
    case MODULE_SOLVED: return "SOLVED";
    case MODULE_STATUS: return "STATUS";
    case MODULE_HEARTBEAT: return "HEARTBEAT";
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

void sendHeartbeat(const uint8_t* data, uint8_t len) {
  if (!canBusInitialized || thisModuleId == 0xFFFF) {
    return;
  }
  
  sendCanMessage(CAN_ID_TIMER, data, len);
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


