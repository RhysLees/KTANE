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

// Special Types
#define CAN_TYPE_BROADCAST 0x3F

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

// Broadcast ID for messages to all modules
#define CAN_ID_BROADCAST CAN_INSTANCE_ID(CAN_TYPE_BROADCAST, 0x00)

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
  AUDIO_SIMON_RED           = 0x0C,
  AUDIO_SIMON_GREEN         = 0x0D,
  AUDIO_SIMON_YELLOW        = 0x0E,
  AUDIO_SIMON_BLUE          = 0x0F,
};

// Commands for Serial Display module
enum CanSerialDisplayCommand : uint8_t
{
  SERIAL_DISPLAY_SET_SERIAL = 0x01,
  SERIAL_DISPLAY_CLEAR = 0x02,
  SERIAL_DISPLAY_SHOW_CREDIT = 0x03
};

// Timer to Module messages
enum TimerToModuleMessage : uint8_t {
  TIMER_GAME_START = 0x10,        // Game started
  TIMER_GAME_STOP = 0x11,         // Game stopped  
  TIMER_STRIKE_UPDATE = 0x12,     // Strike count changed [strikes]
  TIMER_SERIAL_NUMBER = 0x13,     // Serial number [6 chars]
  TIMER_RESET = 0x14,             // Reset module
  TIMER_TIME_UPDATE = 0x15,       // Time remaining [4 bytes, ms]
  TIMER_COUNTDOWN = 0x16,         // Initialization countdown [seconds]
  TIMER_MODULE_DISCOVERED = 0x17  // Module discovered acknowledgment
};

// Module to Timer messages
enum ModuleToTimerMessage : uint8_t {
  MODULE_REGISTER = 0x20,         // Register with timer
  MODULE_SOLVED = 0x21,           // Module was solved
  MODULE_STRIKE = 0x22,           // Module caused a strike  
  MODULE_STATUS = 0x23,           // Status update
  MODULE_HEARTBEAT = 0x24         // Periodic heartbeat
};

// ID negotiation system
enum IdMessage : uint8_t {
  ID_PROBE = 0x01,        // "Anyone using this ID?"
  ID_TAKEN = 0x02         // "Yes, I'm using this ID"
};

// ID negotiation configuration
#define ID_PROBE_TIMEOUT_MS 500
#define ID_MAX_INSTANCE 0x1F

// Function declarations
typedef void (*CanMessageCallback)(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);
typedef void (*RawCanMessageCallback)(uint16_t receiverId, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp);

void initCanBus(uint16_t fullCanId);
void handleCanMessages();
void sendCanMessage(uint16_t receiverID, const uint8_t* data, uint8_t dataLen);
void registerCanCallback(CanMessageCallback callback);
void registerRawCanCallback(RawCanMessageCallback callback);

// ID negotiation functions
bool negotiateInstanceId(uint8_t moduleType, uint8_t* assignedId);
bool assignUniqueId(uint8_t moduleType);
void updateCanId(uint16_t newCanId);
uint8_t getCurrentInstanceId();
uint16_t getCurrentModuleId();

// Module communication helpers
void updateModuleConnections();

// Utility functions
const char* getModuleTypeName(uint8_t moduleType);
const char* getMessageTypeName(uint8_t msgType);
