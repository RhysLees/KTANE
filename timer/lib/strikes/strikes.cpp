#include <Arduino.h>
#include <Wire.h>

#include <can_bus.h>
#include <strikes.h>
#include <game_state.h>

extern GameStateManager gameState;

#define HT16K33_ALPHA_ADDRESS 0x74

static unsigned long lastStrikeBlink = 0;
static bool strikeVisible = true;

uint16_t getCustomChar(char c)
{
  switch (c)
  {
  case 'X':
    return 0x280A; // Segments 1, 3, 11, 13 add 4 and 9 to match game -X-
  case ' ':
    return 0x0000;
  default:
    return 0x0000; // fallback (blank)
  }
}

void writeAlphaRaw(uint8_t digit, uint16_t segments)
{
  Wire.beginTransmission(HT16K33_ALPHA_ADDRESS);
  Wire.write(digit * 2);       // Digit register
  Wire.write(segments & 0xFF); // Low byte
  Wire.write((segments >> 8)); // High byte
  Wire.endTransmission();
}

void clearAlphaDisplay()
{
  Wire.beginTransmission(HT16K33_ALPHA_ADDRESS);
  Wire.write(0x00);
  for (int i = 0; i < 16; i++)
  {
    Wire.write(0x00);
  }
  Wire.endTransmission();
}

void initStrikeDisplay()
{
  Wire.beginTransmission(HT16K33_ALPHA_ADDRESS);
  Wire.write(0x21);
  Wire.endTransmission(); // Oscillator on
  Wire.beginTransmission(HT16K33_ALPHA_ADDRESS);
  Wire.write(0x81);
  Wire.endTransmission(); // Display on
  Wire.beginTransmission(HT16K33_ALPHA_ADDRESS);
  Wire.write(0xE0 | 0x0F);
  Wire.endTransmission(); // Brightness max

  clearAlphaDisplay();
}

void updateAlphaDisplay(const char *txt)
{
  uint16_t left = 0x0000;
  uint16_t right = 0x0000;

  size_t len = strlen(txt);
  if (len == 1)
  {
    left = getCustomChar(txt[0]);
  }
  else if (len >= 2)
  {
    left = getCustomChar(txt[0]);
    right = getCustomChar(txt[1]);
  }

  writeAlphaRaw(2, left);  // Left digit
  writeAlphaRaw(1, right); // Right digit
}

void updateStrikeCount()
{
  uint8_t strikes = gameState.getStrikes();
  static uint8_t lastStrikes = 255;
  static char display[3] = "  ";

  if (strikes != lastStrikes)
  {
    lastStrikes = strikes;
    if (strikes > 0)
    {
      uint8_t payload[1] = {AUDIO_STRIKE};
      sendCanMessage(CAN_ID_AUDIO, payload, 1);
    }
  }

  if (strikes == 0)
  {
    display[0] = ' ';
    display[1] = ' ';
  }
  else if (strikes == 1)
  {
    display[0] = 'X';
    display[1] = ' ';
  }
  else
  {
    display[0] = 'X';
    display[1] = 'X';
  }

  if (strikes < 2)
  {
    updateAlphaDisplay(display);
    return;
  }

  unsigned long now = millis();
  if (now - lastStrikeBlink >= 125)
  {
    lastStrikeBlink = now;
    strikeVisible = !strikeVisible;
  }

  updateAlphaDisplay(strikeVisible ? display : "  ");
}
