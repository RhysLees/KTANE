#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h"
#include "can_bus.h"

#define CAN_SPI_PIN 17
#define CAN_INT_PIN 20

volatile bool canInterruptFlag = false;
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
}

void initCanBus(uint16_t fullCanId) {
  thisModuleId = fullCanId;

  if (CAN.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) == CAN_OK) {
    Serial.println("CAN init OK");
    
    CAN.setMode(MCP_NORMAL);
    
    pinMode(CAN_INT_PIN, INPUT);
    attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
    
    canBusInitialized = true;
    
    Serial.print("CAN module ID set to 0x");
    Serial.println(thisModuleId, HEX);
  } else {
    Serial.println("CAN init FAIL - continuing without CAN bus");
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
  if (!canBusInitialized) return;
  if (!canInterruptFlag) return;
  canInterruptFlag = false;

  if (CAN.checkReceive() == CAN_MSGAVAIL) {
    long unsigned int id;
    unsigned char len = 0;
    unsigned char buf[8];

    CAN.readMsgBuf(&id, &len, buf);

    printCanMessage(id, buf, len);

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
    if (id != thisModuleId && id != CAN_ID_BROADCAST) return;

    for (uint8_t i = 0; i < callbackCount; i++) {
      if (canCallbacks[i]) {
        canCallbacks[i](id, buf, len);
      }
    }
  }
}

void sendCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
  if (!canBusInitialized) {
    // CAN bus not initialized, don't send message
    return;
  }
  
  CAN.sendMsgBuf(id, 0, len, (byte*)data);
  printCanMessage(id, data, len, true);
}

inline void printCanMessage(uint16_t id, const uint8_t* data, uint8_t len, bool sent) {
  if (sent) {
    Serial.print("CAN Sent [0x");
  } else {
    Serial.print("CAN Received [0x");
  }

  Serial.print(id, HEX);
  Serial.print("]");

  for (uint8_t i = 0; i < len; i++) {
    Serial.print(" 0x");
    if (data[i] < 0x10) Serial.print("0");
    Serial.print(data[i], HEX);
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
      // Send probe message to global channel
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      sendCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), probeData, 3);
      
      // Wait for responses with jitter
      idConflictDetected = false;
      int probeDelay = ID_PROBE_TIMEOUT_MS + random(0, 100); // Add jitter
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
      
      // Small delay between probes
      delay(50 + random(0, 50));
    }
    
    if (idAvailable) {
      // Final verification probe before claiming
      Serial.print("ID Negotiation: Final verification for ID ");
      Serial.println(candidateId);
      
      uint8_t probeData[3] = {ID_PROBE, moduleType, candidateId};
      sendCanMessage(CAN_INSTANCE_ID(moduleType, 0x00), probeData, 3);
      
      idConflictDetected = false;
      unsigned long startTime = millis();
      while (millis() - startTime < ID_PROBE_TIMEOUT_MS) {
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
  
  // Random delay to prevent simultaneous negotiation
  int randomDelay = random(100, 2000); // 100-2000ms delay
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
