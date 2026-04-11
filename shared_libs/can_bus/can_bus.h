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
constexpr uint8_t CAN_TYPE_TIMER = 0x00;
constexpr uint8_t CAN_TYPE_AUDIO = 0x01;
// 0x02–0x0F reserved for future core infrastructure modules

// Classic gameplay modules (0x10–0x1F)
constexpr uint8_t CAN_TYPE_WIRES = 0x10;
constexpr uint8_t CAN_TYPE_BUTTON = 0x11;
constexpr uint8_t CAN_TYPE_KEYPAD = 0x12;
constexpr uint8_t CAN_TYPE_SIMON = 0x13;
constexpr uint8_t CAN_TYPE_WHOS = 0x14;
constexpr uint8_t CAN_TYPE_MEMORY = 0x15;
constexpr uint8_t CAN_TYPE_MORSE = 0x16;
constexpr uint8_t CAN_TYPE_COMPLICATED_WIRES = 0x17;
constexpr uint8_t CAN_TYPE_WIRE_SEQUENCES = 0x18;
constexpr uint8_t CAN_TYPE_MAZE = 0x19;
constexpr uint8_t CAN_TYPE_PASSWORD = 0x1A;
// 0x1B–0x1F reserved for future classic gameplay modules

// Side Modules (e.g. bomb casing elements, 0x20–0x2F)
constexpr uint8_t CAN_TYPE_SERIAL_DISPLAY = 0x20;
constexpr uint8_t CAN_TYPE_INDICATOR_PANEL = 0x21;
constexpr uint8_t CAN_TYPE_BATTERY_HOLDER = 0x22;
constexpr uint8_t CAN_TYPE_PORT_PANEL = 0x23;
// 0x24–0x2F reserved for future side modules

// Needy Modules (0x30–0x3B)
constexpr uint8_t CAN_TYPE_VENTING_GAS = 0x30;
constexpr uint8_t CAN_TYPE_CAPACITOR_DISCHARGE = 0x31;
constexpr uint8_t CAN_TYPE_KNOB = 0x32;
// 0x33–0x3B reserved for future needy modules

// Diagnostic & special types (0x3C–0x3F)
constexpr uint8_t CAN_TYPE_DEBUGGER = 0x3C;
constexpr uint8_t CAN_TYPE_DEBUGGER_SENDER = 0x3D;
constexpr uint8_t CAN_TYPE_TOOLING_RESERVED = 0x3E;
constexpr uint8_t CAN_TYPE_BROADCAST = 0x3F;

constexpr uint8_t CAN_MODULE_TYPES[] = {
  CAN_TYPE_TIMER,
  CAN_TYPE_AUDIO,
  CAN_TYPE_WIRES,
  CAN_TYPE_BUTTON,
  CAN_TYPE_KEYPAD,
  CAN_TYPE_SIMON,
  CAN_TYPE_WHOS,
  CAN_TYPE_MEMORY,
  CAN_TYPE_MORSE,
  CAN_TYPE_COMPLICATED_WIRES,
  CAN_TYPE_WIRE_SEQUENCES,
  CAN_TYPE_MAZE,
  CAN_TYPE_PASSWORD,
  CAN_TYPE_SERIAL_DISPLAY,
  CAN_TYPE_INDICATOR_PANEL,
  CAN_TYPE_BATTERY_HOLDER,
  CAN_TYPE_PORT_PANEL,
  CAN_TYPE_VENTING_GAS,
  CAN_TYPE_CAPACITOR_DISCHARGE,
  CAN_TYPE_KNOB,
  CAN_TYPE_DEBUGGER,
  CAN_TYPE_DEBUGGER_SENDER,
  CAN_TYPE_TOOLING_RESERVED,
  CAN_TYPE_BROADCAST
};

constexpr bool validateCanModuleTypes() {
  const unsigned count = sizeof(CAN_MODULE_TYPES);
  for (unsigned i = 0; i < count; ++i) {
    if (CAN_MODULE_TYPES[i] > 0x3F) {
      return false;
    }
    for (unsigned j = i + 1; j < count; ++j) {
      if (CAN_MODULE_TYPES[i] == CAN_MODULE_TYPES[j]) {
        return false;
      }
    }
  }
  return true;
}

static_assert(validateCanModuleTypes(), "CAN module type IDs must be unique and fit within 6 bits");

// Telemetry helpers (MODULE_STATUS payloads with high-bit flag)
#define MODULE_TELEMETRY_FLAG 0x80

// Build unique CAN ID
constexpr uint16_t CAN_INSTANCE_ID(uint8_t moduleType, uint8_t instanceId) {
  return static_cast<uint16_t>(((moduleType & 0x3F) << 5) | (instanceId & 0x1F));
}

// Fixed CAN IDs for unique modules
constexpr uint16_t CAN_ID_TIMER = CAN_INSTANCE_ID(CAN_TYPE_TIMER, 0x00);
constexpr uint16_t CAN_ID_AUDIO = CAN_INSTANCE_ID(CAN_TYPE_AUDIO, 0x00);
constexpr uint16_t CAN_ID_SERIAL_DISPLAY = CAN_INSTANCE_ID(CAN_TYPE_SERIAL_DISPLAY, 0x00);
constexpr uint16_t CAN_ID_INDICATOR_PANEL = CAN_INSTANCE_ID(CAN_TYPE_INDICATOR_PANEL, 0x00);
constexpr uint16_t CAN_ID_BATTERY_HOLDER = CAN_INSTANCE_ID(CAN_TYPE_BATTERY_HOLDER, 0x00);
constexpr uint16_t CAN_ID_PORT_PANEL = CAN_INSTANCE_ID(CAN_TYPE_PORT_PANEL, 0x00);
constexpr uint16_t CAN_ID_DEBUGGER = CAN_INSTANCE_ID(CAN_TYPE_DEBUGGER, 0x00);
constexpr uint16_t CAN_ID_DEBUGGER_SENDER = CAN_INSTANCE_ID(CAN_TYPE_DEBUGGER_SENDER, 0x00);
constexpr uint16_t CAN_ID_TOOLING_RESERVED = CAN_INSTANCE_ID(CAN_TYPE_TOOLING_RESERVED, 0x00);

// Broadcast ID for messages to all modules
constexpr uint16_t CAN_ID_BROADCAST = CAN_INSTANCE_ID(CAN_TYPE_BROADCAST, 0x00);

// ID negotiation configuration
#define ID_PROBE_TIMEOUT_MS 500
#define ID_MAX_INSTANCE 0x1F

// Commands for Serial Display module
enum CanSerialDisplayCommand : uint8_t
{
  SERIAL_DISPLAY_SET_SERIAL = 0x30,
  SERIAL_DISPLAY_CLEAR = 0x31,
  SERIAL_DISPLAY_SHOW_CREDIT = 0x32
};

// Audio messages
 enum AudioMessage : uint8_t {
   AUDIO_SET_VOLUME          = 0x60, // data[1] = volume percent (0-100)
  AUDIO_PLAY_TITLE               = 0x40,
  AUDIO_PLAY_BEEP_NORMAL         = 0x41,
  AUDIO_PLAY_BEEP_FAST           = 0x42,
  AUDIO_PLAY_BEEP_HIGH           = 0x43,
  AUDIO_PLAY_STRIKE              = 0x44,
  AUDIO_PLAY_DEFUSED             = 0x45,
  AUDIO_PLAY_EXPLODED            = 0x46,
  AUDIO_PLAY_CORRECT_TIME        = 0x47,
  AUDIO_PLAY_GAME_OVER_FANFARE   = 0x48,
  AUDIO_PLAY_ALARM_CLOCK_BEEP    = 0x49,
  AUDIO_PLAY_ALARM_CLOCK_SNOOZE  = 0x4A,
  AUDIO_PLAY_ALARM_EMERGENCY     = 0x4B,
  AUDIO_PLAY_SIMON_RED           = 0x4C,
  AUDIO_PLAY_SIMON_GREEN         = 0x4D,
  AUDIO_PLAY_SIMON_YELLOW        = 0x4E,
  AUDIO_PLAY_SIMON_BLUE          = 0x4F,
};

// Timer to Module messages
enum TimerToModuleMessage : uint8_t {
  TIMER_GAME_START = 0x10,        // Game started
  TIMER_GAME_STOP = 0x11,         // Game stopped  
  TIMER_STRIKES = 0x12,            // Strike count [strikes]
  TIMER_SERIAL_NUMBER_FIRST_HALF = 0x13,  // Serial number first 3 chars [3 chars]
  TIMER_SERIAL_NUMBER_LAST_HALF = 0x14,   // Serial number last 3 chars [3 chars]
  TIMER_RESET = 0x15,             // Reset module
  TIMER_TIME = 0x16,              // Time remaining [4 bytes, ms]
  TIMER_COUNTDOWN = 0x17,         // Initialization countdown [seconds]
  TIMER_MODULE_DISCOVERED = 0x18  // Module discovered acknowledgment
};

// Module to Timer messages
enum ModuleToTimerMessage : uint8_t {
  MODULE_REGISTER = 0x20,         // Register with timer
  MODULE_SOLVED = 0x21,           // Module was solved
  MODULE_STRIKE = 0x22,           // Module caused a strike  
  MODULE_STATUS = 0x23,           // Status update
  MODULE_HEARTBEAT = 0x24,        // Periodic heartbeat
  MODULE_TELEMETRY = 0x25         // Telemetry payload
};


// Shared module status flags for enhanced heartbeats and telemetry
enum ModuleStatus : uint8_t {
  MODULE_STATUS_IDLE = 0x00,
  MODULE_STATUS_ACTIVE = 0x01,
  MODULE_STATUS_SOLVED = 0x02,
  MODULE_STATUS_ARMED = 0x03,      // For needy modules
  MODULE_STATUS_ERROR = 0xFF
};

enum ModuleTelemetryType : uint8_t {
  MODULE_TELEMETRY_GENERAL = 0x40,   // Standard status/progress update
  MODULE_TELEMETRY_CUSTOM0 = 0x41,   // Reserved for module-specific data
  MODULE_TELEMETRY_CUSTOM1 = 0x42,   // Reserved for module-specific data
  MODULE_TELEMETRY_CUSTOM2 = 0x43    // Reserved for module-specific data
};

// ID negotiation system
enum IdMessage : uint8_t {
  ID_PROBE = 0x01,        // "Anyone using this ID?"
  ID_TAKEN = 0x02         // "Yes, I'm using this ID"
};

// Function declarations
typedef void (*CanMessageCallback)(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);
typedef void (*RawCanMessageCallback)(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp);

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
void decodeCanId(uint16_t canId, uint8_t* moduleType, uint8_t* instanceId);
void logCanMessage(const char* direction, uint16_t receiverId, uint16_t senderId, uint16_t decodeId, const uint8_t* data, uint8_t len);