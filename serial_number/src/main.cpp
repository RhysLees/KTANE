#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <can_bus.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <epaper.h>
#include <module_state.h>

// Callback function called when serial number is received from timer via module_state
void onSerialNumberReceived(const String& serial) {
  if (serial.length() > 0) {
    epaperDrawTag(serial);
  }
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len) {
  // Let module_state handle timer messages first
  moduleStateHandleCanMessage(id, senderId, data, len);
  
  // Handle serial display-specific messages
  if (id == CAN_ID_SERIAL_DISPLAY && len >= 1) {
    uint8_t command = data[0];
    
    // Set module state to active when processing display commands
    setModuleStateStatus(MODULE_STATUS_ACTIVE);

    switch (command) {
      // SERIAL_DISPLAY_SET_SERIAL is now handled by module_state callback
      // Keeping this as a manual override option for testing
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
    setModuleStateStatus(MODULE_STATUS_IDLE);
  }
}

void setup() {
  Serial.begin(115200);
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  epaperInit();

  initCanBus(CAN_ID_SERIAL_DISPLAY);
  registerCanCallback(onCanMessage);

  // Initialize module_state system (starts in discovery mode)
  initModuleState(MODULE_STATE_NO_LED);  // Serial display module doesn't have a status LED
  
  // Register callback to receive serial number from timer
  if (globalModuleState) {
    globalModuleState->setSerialNumberCallback(onSerialNumberReceived);
    
    // Check if serial number is already available (e.g., from previous game)
    String existingSerial = globalModuleState->getSerialNumber();
    if (existingSerial.length() > 0) {
      epaperDrawTag(existingSerial);
    }
  }

  delay(5000);
}

void loop() {
  handleCanMessages();
  updateModuleState();
}
