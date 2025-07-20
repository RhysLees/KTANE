#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <can_bus.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <epaper.h>
#include <heartbeat.h>

void handleSerialDisplayMessage(uint16_t id, const uint8_t *data, uint8_t len) {
  if (id != CAN_ID_SERIAL_DISPLAY || len < 3)
    return;

  uint8_t senderType = data[0];
  uint8_t senderInstance = data[1];
  uint8_t command = data[2];
  
  // Set heartbeat to active when processing display commands
  setHeartbeatStatus(MODULE_STATUS_ACTIVE);

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
  
  // Return to idle status after processing
  setHeartbeatStatus(MODULE_STATUS_IDLE);
}

void setup() {
  Serial.begin(115200);
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  epaperInit();

  initCanBus(CAN_ID_SERIAL_DISPLAY);
  registerCanCallback(handleSerialDisplayMessage);

  // Initialize heartbeat system for serial display
  initHeartbeat(HEARTBEAT_INTERVAL_SERIAL_DISPLAY);

  Serial.println("Serial display ready with heartbeat system");
  delay(5000);
}

void loop() {
  handleCanMessages();
  updateHeartbeat();
}
