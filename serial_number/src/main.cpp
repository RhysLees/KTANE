#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <can_bus.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <epaper.h>

// Heartbeat timing
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 3000; // 3 seconds

void handleSerialDisplayMessage(uint16_t id, const uint8_t *data, uint8_t len) {
  if (id != CAN_ID_SERIAL_DISPLAY || len < 3)
    return;

  uint8_t senderType = data[0];
  uint8_t senderInstance = data[1];
  uint8_t command = data[2];

  switch (command) {
    case SERIAL_DISPLAY_SET_SERIAL:
      if (len >= 9) {
        char serial[7];
        memcpy(serial, &data[3], 6);
        serial[6] = '\0';
        epaperDrawTag(String(serial));
      }
      break;

    case SERIAL_DISPLAY_CLEAR:
      epaperClear();
      break;

    case SERIAL_DISPLAY_SHOW_CREDIT:
      epaperDrawCredit();
      break;

    default:
      break;
  }
}

void sendHeartbeat() {
  unsigned long now = millis();
  if (now - lastHeartbeat >= HEARTBEAT_INTERVAL) {
    uint8_t heartbeatData[1] = {MODULE_HEARTBEAT};
    sendCanMessage(CAN_ID_TIMER, heartbeatData, 1);
    lastHeartbeat = now;
  }
}

void setup() {
  Serial.begin(115200);
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  epaperInit();

  initCanBus(CAN_ID_SERIAL_DISPLAY);
  registerCanCallback(handleSerialDisplayMessage);

  Serial.println("Serial display ready - sending heartbeats every 3s");
  delay(5000);
}

void loop() {
  handleCanMessages();
  sendHeartbeat();
}
