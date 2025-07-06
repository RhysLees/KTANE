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
  // Note: Keep this minimal - we're in an interrupt context
}

void initCanBus(uint16_t fullCanId) {
  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN init OK");
  } else {
    Serial.println("CAN init FAIL");
    delay(1000);
    initCanBus(fullCanId);
  }

  // CAN.setMode(MCP_NORMAL);

  pinMode(CAN_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
  
  // Set the CAN bus as initialized
  canBusInitialized = true;
  
  // Set the initial module ID (for fixed ID modules or as starting point for negotiation)
  thisModuleId = fullCanId;
  
  Serial.print("CAN bus initialized with ID: 0x");
  Serial.println(thisModuleId, HEX);
  Serial.print("DEBUG: Input fullCanId was: 0x");
  Serial.println(fullCanId, HEX);
  Serial.print("DEBUG: Module type: 0x");
  Serial.println((thisModuleId >> 5) & 0x7F, HEX);
  Serial.print("DEBUG: Instance ID: 0x");
  Serial.println(thisModuleId & 0x1F, HEX);
}

void registerCanCallback(CanMessageCallback callback) {
  if (callbackCount < MAX_CAN_CALLBACKS) {
    canCallbacks[callbackCount++] = callback;
  }
}

void handleIdNegotiation(uint16_t id, const uint8_t* buf, uint8_t len) {
  // Handle ID negotiation messages on module-specific global channels only
  // (instance 0x00 but NOT the global broadcast channel)
  if (len >= 3 && (id & 0x1F) == 0x00 && id != CAN_ID_BROADCAST) {
    uint8_t msgType = buf[0];
    uint8_t moduleType = buf[1];
    uint8_t instanceId = buf[2];
    
    if (msgType == ID_PROBE && moduleType == currentModuleType) {
      if (instanceId == currentInstanceId && currentInstanceId != 0) {
        uint8_t takenData[3] = {ID_TAKEN, moduleType, instanceId};
        // Use direct CAN.sendMsgBuf to maintain consistent format with probes
        uint8_t sendResult = CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)takenData);
        Serial.print("CAN.sendMsgBuf result: ");
        Serial.println(sendResult);
        printCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), takenData, 3, true);
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

    // Always log all received messages with ! prefix
    Serial.print("!");
    printCanMessage(id, buf, len, false);

    // Handle ID negotiation messages first (before any filtering)
    handleIdNegotiation(id, buf, len);

    // Update connection status for fixed modules
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

    // Call registered callbacks for messages addressed to this module or broadcast
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
  
  // Check if target module is connected (only for fixed modules)
  // Temporarily disabled for debugging physical layer
  /*
  if (receiverID == CAN_ID_AUDIO && !audioModuleConnected) {
    Serial.println("CAN: Audio module not connected - message skipped");
    return;
  }
  if (receiverID == CAN_ID_SERIAL_DISPLAY && !serialDisplayConnected) {
    Serial.println("CAN: Serial Display module not connected - message skipped");
    return;
  }
  */
  
  // Build standardized message: [senderType, senderInstance, messageType, ...messageData]
  uint8_t fullMessage[8];
  uint8_t senderType = (thisModuleId >> 5) & 0x7F;
  uint8_t senderInstance = thisModuleId & 0x1F;
  
  Serial.print("DEBUG: Sending message - thisModuleId=0x");
  Serial.print(thisModuleId, HEX);
  Serial.print(", senderType=0x");
  Serial.print(senderType, HEX);
  Serial.print(", senderInstance=0x");
  Serial.println(senderInstance, HEX);
  
  fullMessage[0] = senderType;
  fullMessage[1] = senderInstance;
  if (dataLen > 0) {
    fullMessage[2] = data[0]; // First byte of data is message type
    
    uint8_t maxDataBytes = min(dataLen - 1, 5);
    for (uint8_t i = 0; i < maxDataBytes; i++) {
      fullMessage[3 + i] = data[1 + i];
    }
    uint8_t totalLen = 3 + maxDataBytes;
    
    uint8_t sendResult = CAN.sendMsgBuf(receiverID, 0, totalLen, (byte*)fullMessage);
    Serial.print("CAN.sendMsgBuf result: ");
    Serial.println(sendResult);
    printCanMessage(receiverID, fullMessage, totalLen, true);
  } else {
    uint8_t sendResult = CAN.sendMsgBuf(receiverID, 0, 2, (byte*)fullMessage);
    Serial.print("CAN.sendMsgBuf result: ");
    Serial.println(sendResult);
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
  if (sent) {
    Serial.print("CAN TX");
  } else {
    Serial.print("CAN RX");
  }
  
  if (sent) {
    printCanIdInfo(thisModuleId, " - S");
    printCanIdInfo(id, " - R");
  } else {
    printCanIdInfo(id, " - S");
    printCanIdInfo(thisModuleId, " - R");
  }
  
  if (len > 0) {
    Serial.print(" - Data:[");
    for (uint8_t i = 0; i < len; i++) {
      if (i > 0) Serial.print(",");
      Serial.print("0x");
      Serial.print(data[i], HEX);
    }
    Serial.print("]");
  }

  Serial.println();
}

bool negotiateInstanceId(uint8_t moduleType, uint8_t* assignedId) {
  Serial.print("CRITICAL ERROR: negotiateInstanceId called with moduleType=0x");
  Serial.println(moduleType, HEX);
  Serial.print("This should NEVER happen for Timer module!");
  Serial.println();
  
  // Prevent Timer module from doing ID negotiation
  if (thisModuleId == CAN_ID_TIMER) {
    Serial.println("ERROR: Timer module should never negotiate IDs - blocking call");
    return false;
  }
  
  if (!canBusInitialized) return false;
  
  currentModuleType = moduleType;
  currentInstanceId = 0;
  
  for (uint8_t candidateId = 0x01; candidateId <= ID_MAX_INSTANCE; candidateId++) {
    bool idAvailable = true;
    for (int probe = 0; probe < 3; probe++) {
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      uint8_t sendResult = CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)probeData);
      Serial.print("CAN.sendMsgBuf result: ");
      Serial.println(sendResult);
      printCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), probeData, 3, true);
      
      idConflictDetected = false;
      int probeDelay = 200 + random(0, 50); // Reduced from 500ms to 200ms + jitter
      unsigned long startTime = millis();
      while (millis() - startTime < probeDelay) {
        handleCanMessages();
        if (idConflictDetected) {
          idAvailable = false;
          break;
        }
      }
      
      if (!idAvailable) break;
      
      delay(20 + random(0, 30)); // Small delay between probes
    }
    
    if (idAvailable) {
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      uint8_t sendResult = CAN.sendMsgBuf(CAN_INSTANCE_ID(moduleType, 0x00), 0, 3, (byte*)probeData);
      Serial.print("CAN.sendMsgBuf result: ");
      Serial.println(sendResult);
      printCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), probeData, 3, true);
      
      idConflictDetected = false;
      unsigned long startTime = millis();
      while (millis() - startTime < 200) { // Reduced from ID_PROBE_TIMEOUT_MS (500ms) to 200ms
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
      int backoffDelay = random(100, 500) * candidateId;
      delay(backoffDelay);
    }
  }
  
  return false;
}

bool assignUniqueId(uint8_t moduleType) {
  Serial.print("CRITICAL ERROR: assignUniqueId called with moduleType=0x");
  Serial.println(moduleType, HEX);
  Serial.print("This should NEVER happen for Timer module!");
  Serial.println();
  
  // Prevent Timer module from doing ID negotiation
  if (thisModuleId == CAN_ID_TIMER) {
    Serial.println("ERROR: Timer module should never negotiate IDs - blocking call");
    return false;
  }
  
  if (!canBusInitialized) return false;
  
  int randomDelay = random(50, 500); // 50-500ms delay (reduced from 100-2000ms)
  delay(randomDelay);
  
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
  return thisModuleId & 0x1F; // Extract instance ID from current CAN ID
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
  
  // Check Audio module timeout
  if (audioModuleConnected && (now - lastAudioPing > MODULE_TIMEOUT_MS)) {
    audioModuleConnected = false;
    Serial.println("CAN: Audio module disconnected (timeout)");
  }
  
  // Check Serial Display module timeout
  if (serialDisplayConnected && (now - lastSerialDisplayPing > MODULE_TIMEOUT_MS)) {
    serialDisplayConnected = false;
    Serial.println("CAN: Serial Display module disconnected (timeout)");
  }
}
