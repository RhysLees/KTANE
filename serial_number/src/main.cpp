#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <can_bus.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <epaper.h>
#include <heartbeat.h>

void handleSerialDisplayMessage(uint16_t id, const uint8_t *data, uint8_t len) {
  if (id != CAN_ID_SERIAL_DISPLAY || len < 1)
    return;

  uint8_t command = data[0];
  
  // Set heartbeat to active when processing display commands
  setHeartbeatStatus(MODULE_STATUS_ACTIVE);

  switch (command) {
    case SERIAL_DISPLAY_SET_SERIAL:
      if (len >= 7) {
        char serial[7];
        memcpy(serial, &data[1], 6);
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
  
  // Handle game state messages from timer
  if (id == CAN_ID_BROADCAST && len >= 1) {
    uint8_t messageType = data[0];
    if (messageType == TIMER_GAME_START) {
      setHeartbeatGameRunning(true);
      Serial.println("Serial Display: Game started - switching to 5s heartbeats");
    } else if (messageType == TIMER_GAME_STOP) {
      setHeartbeatGameRunning(false);
      Serial.println("Serial Display: Game stopped - switching to 1s heartbeats");
    }
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

  // Initialize heartbeat system (starts in discovery mode)
  initHeartbeat();

  Serial.println("Serial display ready with dynamic heartbeat system");
  delay(5000);
}

void loop() {
  handleCanMessages();
  updateHeartbeat();
}
