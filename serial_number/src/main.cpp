#include <Arduino.h>
#include <Wire.h>
#include <SPI.h>
#include <can_bus.h>
#include <GxEPD2_3C.h>
#include <Adafruit_GFX.h>
#include <epaper.h>

// Handle incoming CAN message
void handleSerialDisplayMessage(uint16_t id, const uint8_t *data, uint8_t len)
{
  if (id != CAN_ID_SERIAL_DISPLAY || len == 0)
    return;

  uint8_t command = data[0];

  switch (command)
  {
  case SERIAL_DISPLAY_SET_SERIAL:
    if (len == 7)
    {
      char serial[7]; // 6 characters + null terminator
      memcpy(serial, &data[1], 6);
      serial[6] = '\0';
      epaperDrawTag(String(serial));
      Serial.print("Set serial: ");
      Serial.println(serial);
    }
    break;

  case SERIAL_DISPLAY_CLEAR:
    epaperClear();
    Serial.println("Display cleared");
    break;

  case SERIAL_DISPLAY_SHOW_CREDIT:
    epaperDrawCredit();
    Serial.println("Credit displayed");
    break;

  default:
    Serial.print("Unknown serial display command: 0x");
    Serial.println(command, HEX);
    break;
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  epaperInit();

  initCanBus(CAN_ID_SERIAL_DISPLAY);
  registerCanCallback(handleSerialDisplayMessage);

  delay(5000);
}

void loop()
{
  handleCanMessages();
}
