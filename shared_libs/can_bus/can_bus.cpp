#include <Arduino.h>
#include <SPI.h>
#include "mcp_can.h"
#include "can_bus.h"

#define CAN_SPI_PIN 17
#define CAN_INT_PIN 20

volatile bool canInterruptFlag = false;
uint16_t thisModuleId = 0xFFFF; // Default uninitialized

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
  } else {
    Serial.println("CAN init FAIL");
    while (1);
  }

  CAN.setMode(MCP_NORMAL);

  pinMode(CAN_INT_PIN, INPUT);
  attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);

  Serial.print("CAN module ID set to 0x");
  Serial.println(thisModuleId, HEX);
}

void registerCanCallback(CanMessageCallback callback) {
  if (callbackCount < MAX_CAN_CALLBACKS) {
    canCallbacks[callbackCount++] = callback;
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

    printCanMessage(id, buf, len);

    // Filter to this module only
    if (id != thisModuleId) return;

    for (uint8_t i = 0; i < callbackCount; i++) {
      if (canCallbacks[i]) {
        canCallbacks[i](id, buf, len);
      }
    }
  }
}

void sendCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
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
