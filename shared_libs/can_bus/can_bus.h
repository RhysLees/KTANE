#pragma once

#include <stdint.h>

/*
  CAN ID Structure (Standard 11-bit):

     0b MMMMMMIIIIII
         |     |
         |     +-- Instance ID (0–31)
         +-------- Module Type (0–63)
*/

// Module Types (6 bits max: 0x00–0x3F)
#define CAN_TYPE_TIMER   0x00
#define CAN_TYPE_AUDIO   0x01
#define CAN_TYPE_WIRES   0x10
#define CAN_TYPE_BUTTON  0x11
#define CAN_TYPE_KEYPAD  0x12
#define CAN_TYPE_SIMON   0x13
#define CAN_TYPE_WHOS    0x14
#define CAN_TYPE_MEMORY  0x15
#define CAN_TYPE_MORSE   0x16
#define CAN_TYPE_COMPLICATED_WIRES 0x17
#define CAN_TYPE_WIRE_SEQUENCES    0x18
#define CAN_TYPE_MAZE    0x19
#define CAN_TYPE_PASSWORD 0x1A

// Needy Modules
#define CAN_TYPE_VENTING_GAS         0x30
#define CAN_TYPE_CAPACITOR_DISCHARGE 0x31
#define CAN_TYPE_KNOB                0x32

// Side Modules (e.g. bomb casing elements)
#define CAN_TYPE_SERIAL_DISPLAY 0x20
#define CAN_TYPE_INDICATOR_PANEL 0x21
#define CAN_TYPE_BATTERY_HOLDER 0x22
#define CAN_TYPE_PORT_PANEL 0x23

// Build unique CAN ID
#define CAN_INSTANCE_ID(moduleType, instanceId) \
  (((moduleType & 0x3F) << 5) | (instanceId & 0x1F))

// Fixed CAN IDs for unique modules
#define CAN_ID_TIMER CAN_INSTANCE_ID(CAN_TYPE_TIMER, 0x00)
#define CAN_ID_AUDIO CAN_INSTANCE_ID(CAN_TYPE_AUDIO, 0x00)
#define CAN_ID_SERIAL_DISPLAY CAN_INSTANCE_ID(CAN_TYPE_SERIAL_DISPLAY, 0x00)
#define CAN_ID_INDICATOR_PANEL CAN_INSTANCE_ID(CAN_TYPE_INDICATOR_PANEL, 0x00)
#define CAN_ID_BATTERY_HOLDER CAN_INSTANCE_ID(CAN_TYPE_BATTERY_HOLDER, 0x00)
#define CAN_ID_PORT_PANEL CAN_INSTANCE_ID(CAN_TYPE_PORT_PANEL, 0x00)

// Audio sound identifiers
enum CanAudioSound : uint8_t {
  AUDIO_BEEP_NORMAL         = 0x01,
  AUDIO_BEEP_FAST           = 0x02,
  AUDIO_BEEP_HIGH           = 0x03,
  AUDIO_STRIKE              = 0x04,
  AUDIO_DEFUSED             = 0x05,
  AUDIO_EXPLODED            = 0x06,
  AUDIO_CORRECT_TIME        = 0x07,
  AUDIO_GAME_OVER_FANFARE   = 0x08,
  AUDIO_ALARM_CLOCK_BEEP    = 0x09,
  AUDIO_ALARM_CLOCK_SNOOZE  = 0x0A,
  AUDIO_ALARM_EMERGENCY     = 0x0B,
  // Simon Says tone frequencies
  AUDIO_SIMON_RED           = 0x10,  // 550Hz
  AUDIO_SIMON_BLUE          = 0x11,  // 660Hz
  AUDIO_SIMON_GREEN         = 0x12,  // 775Hz
  AUDIO_SIMON_YELLOW        = 0x13   // 985Hz
};

// Commands for Serial Display module
enum CanSerialDisplayCommand : uint8_t
{
  SERIAL_DISPLAY_SET_SERIAL = 0x01, // Set serial number (e.g., "C4R7A2")
  SERIAL_DISPLAY_CLEAR = 0x02,      // Clear the display
  SERIAL_DISPLAY_SHOW_CREDIT = 0x03 // Show developer credit
};

// Function declarations
typedef void (*CanMessageCallback)(uint16_t id, const uint8_t* data, uint8_t len);
void initCanBus(uint16_t fullCanId);
void handleCanMessages();
void sendCanMessage(uint16_t id, const uint8_t* data, uint8_t len);
void registerCanCallback(CanMessageCallback callback);

// Debugging helper
inline void printCanMessage(uint16_t id, const uint8_t* data, uint8_t len, bool sent = false);
