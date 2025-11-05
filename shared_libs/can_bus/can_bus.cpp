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
// Raw CAN callbacks are called for all messages, regardless of module ID

#define MAX_RAW_CAN_CALLBACKS 2
static RawCanMessageCallback rawCanCallbacks[MAX_RAW_CAN_CALLBACKS];
static uint8_t rawCallbackCount = 0;

void onCanInterrupt() {
  canInterruptFlag = true;
  canInterruptCount++;
}

void initCanBus(uint16_t fullCanId) {
  SPI.begin();
  
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    CAN.init_Mask(0, 0, 0x00000000);
    CAN.init_Mask(1, 0, 0x00000000);
    
    for (int i = 0; i < 6; i++) {
      CAN.init_Filt(i, 0, 0x00000000);
    }

    CAN.enOneShotTX();
    CAN.setMode(MCP_NORMAL);
  } else {
    delay(1000);
    initCanBus(fullCanId);
  }

  pinMode(CAN_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
  
  canBusInitialized = true;
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
  if (!digitalRead(CAN_INT_PIN) || canInterruptFlag) {
    canInterruptFlag = false;
    
    while (CAN.checkReceive() == CAN_MSGAVAIL) {
      long unsigned int id;
      unsigned char len = 0;
      unsigned char buf[8];

      CAN.readMsgBuf(&id, &len, buf);

      handleIdNegotiation(id, buf, len);

      uint16_t senderId = 0;
      uint8_t shiftedData[8];
      uint8_t shiftedLen = len;
      
      if (len >= 2) {
        senderId = (buf[0] << 8) | buf[1];
        shiftedLen = len - 2;
        memcpy(shiftedData, &buf[2], shiftedLen);
      } else {
        shiftedLen = len;
        memcpy(shiftedData, buf, len);
      }

      unsigned long timestamp = millis();
      for (uint8_t i = 0; i < rawCallbackCount; i++) {
        if (rawCanCallbacks[i]) {
          rawCanCallbacks[i](id, senderId, shiftedData, shiftedLen, timestamp);
        }
      }

      if (id != thisModuleId && id != CAN_ID_BROADCAST) {
        continue;
      }

      for (uint8_t i = 0; i < callbackCount; i++) {
        if (canCallbacks[i]) {
          canCallbacks[i](id, senderId, shiftedData, shiftedLen);
        }
      }

      // Decode and log the CAN message
      if (senderId != 0) {
        logCanMessage("RX", id, senderId, senderId, shiftedData, shiftedLen);
      }
      
    }
  }
}

void sendCanMessage(uint16_t receiverId, const uint8_t* data, uint8_t dataLen) {
  if (!canBusInitialized) {
    return;
  }
  
  if (dataLen > 0 && dataLen <= 6) {
    uint8_t messageData[8];
    
    messageData[0] = (thisModuleId >> 8) & 0xFF;
    messageData[1] = thisModuleId & 0xFF;
    
    memcpy(&messageData[2], data, dataLen);
    
    byte sendStatus = CAN.sendMsgBuf(receiverId, 0, dataLen + 2, (byte*)messageData);
    if (sendStatus != CAN_OK) {
      Serial.println("Failed to send CAN message");
      return;
    }

    logCanMessage("TX", receiverId, thisModuleId, receiverId, data, dataLen);
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
    return false;
  }
  
  if (!canBusInitialized) return false;
  
  currentModuleType = moduleType;
  currentInstanceId = 0;
  
  for (uint8_t candidateId = 0x01; candidateId <= ID_MAX_INSTANCE; candidateId++) {
    bool idAvailable = true;
    
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
    
    if (candidateId >= 3) {
      delay(random(100, 500) * candidateId);
    }
  }
  
  return false;
}

bool assignUniqueId(uint8_t moduleType) {
  if (thisModuleId == CAN_ID_TIMER) {
    return false;
  }
  
  if (!canBusInitialized) return false;
  
  delay(random(50, 500));
  
  uint8_t assignedId = 0;
  if (negotiateInstanceId(moduleType, &assignedId)) {
    uint16_t finalCanId = CAN_INSTANCE_ID(moduleType, assignedId);
    updateCanId(finalCanId);
    return true;
  } else {
    uint16_t defaultCanId = CAN_INSTANCE_ID(moduleType, 0x01);
    updateCanId(defaultCanId);
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
  }
  
  if (serialDisplayConnected && (now - lastSerialDisplayPing > MODULE_TIMEOUT_MS)) {
    serialDisplayConnected = false;
  }
}

void decodeCanId(uint16_t canId, uint8_t* moduleType, uint8_t* instanceId) {
  *moduleType = (canId >> 5) & 0x3F;
  *instanceId = canId & 0x1F;
}

void logCanMessage(const char* direction, uint16_t receiverId, uint16_t senderId, uint16_t decodeId, const uint8_t* data, uint8_t len) {
  if (decodeId == 0) {
    return;
  }
  
  uint8_t moduleType, instanceId;
  decodeCanId(decodeId, &moduleType, &instanceId);
  
  Serial.print("CAN ");
  Serial.print(direction);
  Serial.print(": ID=0x");
  Serial.print(receiverId, HEX);
  Serial.print(" | Sender=0x");
  Serial.print(senderId, HEX);
  Serial.print(" | Module=");
  Serial.print(getModuleTypeName(moduleType));
  Serial.print(" | Instance=");
  Serial.print(instanceId);
  Serial.print(" | Len=");
  Serial.print(len);
  
  // Decode command if data is available
  if (len > 0) {
    const char* commandName = getMessageTypeName(data[0]);
    Serial.print(" | Command=");
    Serial.print(commandName);
    Serial.print(" (0x");
    if (data[0] < 0x10) Serial.print("0");
    Serial.print(data[0], HEX);
    Serial.print(")");
  }
  
  Serial.print(" | Data=");
  
  for (uint8_t i = 0; i < len; i++) {
    Serial.print("0x");
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
    if (i < len - 1) Serial.print(" ");
  }
  Serial.println();
}