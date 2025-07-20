# CAN Bus Library

The foundational communication library for the KTANE project, handling all inter-module communication via CAN bus.

## Overview

The CAN bus library provides a standardized communication protocol for KTANE modules to:
- Exchange messages reliably between modules
- Negotiate unique module IDs automatically  
- Detect module connections and handle timeouts
- Route messages efficiently with filtering
- Support both fixed and dynamic module addressing

## Features

- **Automatic ID Negotiation**: Dynamic modules negotiate unique IDs on startup
- **Connection Detection**: Tracks module presence and detects disconnections
- **Message Filtering**: Only receives relevant messages to reduce processing overhead
- **Standardized Format**: Consistent message structure across all modules
- **Callback System**: Easy message handling with registered callbacks
- **Debug Support**: Built-in utilities for troubleshooting and monitoring

## Hardware Requirements

- **CAN Controller**: MCP2515 SPI-based CAN controller
- **CAN Transceiver**: MCP2551 or compatible
- **SPI Connection**: CS pin (configurable), MOSI, MISO, SCK
- **Interrupt Pin**: For efficient message reception
- **Termination**: 120Ω resistors at both ends of CAN bus

### Default Pin Configuration
```cpp
#define CAN_SPI_PIN 17    // SPI CS pin
#define CAN_INT_PIN 20    // Interrupt pin
```

## Quick Start

### Basic Setup
```cpp
#include <can_bus.h>

void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle incoming messages
    if (len >= 3) {
        uint8_t senderType = data[0];
        uint8_t senderInstance = data[1]; 
        uint8_t messageType = data[2];
        
        switch (messageType) {
            case TIMER_GAME_START:
                // Handle game start
                break;
            // ... other message types
        }
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize CAN bus
    initCanBus(CAN_ID_AUDIO);  // For fixed ID module
    registerCanCallback(onCanMessage);
}

void loop() {
    handleCanMessages();  // Process incoming messages
}
```

### Dynamic ID Module Setup
```cpp
void setup() {
    Serial.begin(115200);
    
    // Start with temporary ID for negotiation
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    registerCanCallback(onCanMessage);
    
    // Negotiate unique ID
    assignUniqueId(CAN_TYPE_SIMON);
    
    // Get final assigned ID
    uint8_t instanceId = getCurrentInstanceId();
    uint16_t canId = getCurrentModuleId();
    
    Serial.print("Assigned Instance ID: ");
    Serial.println(instanceId);
}
```

## API Reference

### Initialization Functions

#### `initCanBus(uint16_t fullCanId)`
Initializes the CAN bus system with specified module ID.
- **Parameters**: `fullCanId` - Complete CAN ID for this module
- **Usage**: Call once in setup()

#### `registerCanCallback(CanMessageCallback callback)`
Registers a callback function for message handling.
- **Parameters**: `callback` - Function to call when messages arrive
- **Callback Signature**: `void callback(uint16_t id, const uint8_t* data, uint8_t len)`

### Message Handling

#### `handleCanMessages()`
Processes incoming CAN messages and calls registered callbacks.
- **Usage**: Call frequently in loop()
- **Returns**: void

#### `sendCanMessage(uint16_t receiverID, const uint8_t* data, uint8_t dataLen)`
Sends a CAN message to specified receiver.
- **Parameters**: 
  - `receiverID` - CAN ID of target module
  - `data` - Message payload
  - `dataLen` - Length of payload
- **Format**: Automatically adds sender information

### ID Management

#### `assignUniqueId(uint8_t moduleType)`
Negotiates a unique instance ID for dynamic modules.
- **Parameters**: `moduleType` - Type of module (e.g., CAN_TYPE_SIMON)
- **Returns**: `bool` - true if successful, false if failed
- **Usage**: Call after initCanBus() for dynamic modules

#### `getCurrentInstanceId()`
Gets the current instance ID for this module.
- **Returns**: `uint8_t` - Instance ID (1-31)

#### `getCurrentModuleId()`
Gets the complete CAN ID for this module.
- **Returns**: `uint16_t` - Full CAN ID

#### `updateCanId(uint16_t newCanId)`
Updates the module's CAN ID (advanced usage).
- **Parameters**: `newCanId` - New CAN ID to use

### Connection Management

#### `updateModuleConnections()`
Checks for module timeouts and updates connection status.
- **Usage**: Call periodically (handled automatically by some modules)

#### `sendHeartbeat(const uint8_t* data, uint8_t len)`
Sends a heartbeat message to the timer module.
- **Note**: Use the heartbeat library instead for standardized heartbeats

### Utility Functions

#### `getModuleTypeName(uint8_t moduleType)`
Returns human-readable name for module type.
- **Parameters**: `moduleType` - Module type code
- **Returns**: `const char*` - Module type name

#### `getMessageTypeName(uint8_t msgType)`
Returns human-readable name for message type.
- **Parameters**: `msgType` - Message type code
- **Returns**: `const char*` - Message type name

## Message Format

### Standard Message Structure
All CAN messages follow this format:
```
Byte 0: Sender Type (module type that sent the message)
Byte 1: Sender Instance (instance ID of sender)  
Byte 2: Message Type (what kind of message this is)
Byte 3-7: Message Data (payload, varies by message type)
```

### CAN ID Structure
```
11-bit CAN ID: MMMMMM IIIII
               |      |
               |      └─ Instance ID (0-31)  
               └──────── Module Type (0-63)
```

### Examples
```cpp
// Timer broadcasts game start (ID 0x7E0 = broadcast)
// Data: [0x00, 0x00, 0x10] = [TIMER, instance 0, GAME_START]

// Simon Says reports solved (sent to timer ID 0x00)  
// Data: [0x13, 0x02, 0x21] = [SIMON, instance 2, SOLVED]

// Audio plays strike sound (sent to audio ID 0x20)
// Data: [0x00, 0x00, 0x04] = [TIMER, instance 0, AUDIO_STRIKE]
```

## Module Types

### Fixed ID Modules
These modules have predetermined IDs:
```cpp
#define CAN_ID_TIMER          0x00   // Timer module
#define CAN_ID_AUDIO          0x20   // Audio system  
#define CAN_ID_SERIAL_DISPLAY 0x400  // Serial number display
#define CAN_ID_BROADCAST      0x7E0  // Broadcast to all modules
```

### Dynamic ID Modules  
These modules negotiate unique IDs:
```cpp
#define CAN_TYPE_SIMON   0x13    // Simon Says
#define CAN_TYPE_WIRES   0x10    // Simple Wires
#define CAN_TYPE_BUTTON  0x11    // Big Button
// ... see can_bus.h for complete list
```

## Message Types

### Timer → Module Messages
- `TIMER_GAME_START` (0x10) - Game has started
- `TIMER_GAME_STOP` (0x11) - Game has stopped
- `TIMER_STRIKE_UPDATE` (0x12) - Strike count changed
- `TIMER_SERIAL_NUMBER` (0x13) - Serial number update
- `TIMER_RESET` (0x14) - Reset all modules
- `TIMER_TIME_UPDATE` (0x15) - Time remaining update
- `TIMER_COUNTDOWN` (0x16) - Initialization countdown

### Module → Timer Messages
- `MODULE_REGISTER` (0x20) - Register with timer system
- `MODULE_SOLVED` (0x21) - Module was successfully solved
- `MODULE_STRIKE` (0x22) - Module caused a strike
- `MODULE_STATUS` (0x23) - Status update
- `MODULE_HEARTBEAT` (0x24) - Periodic heartbeat

### ID Negotiation Messages
- `ID_PROBE` (0x01) - Check if ID is available
- `ID_TAKEN` (0x02) - Respond that ID is in use

### Audio Messages
- `AUDIO_BEEP_NORMAL` (0x01) - Normal beep sound
- `AUDIO_STRIKE` (0x04) - Strike sound
- `AUDIO_DEFUSED` (0x05) - Bomb defused sound
- `AUDIO_SIMON_RED` (0x0C) - Simon Says red tone
- ... see can_bus.h for complete list

## Advanced Usage

### Multiple Callbacks
```cpp
void primaryHandler(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle primary message types
}

void secondaryHandler(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle secondary message types
}

void setup() {
    initCanBus(CAN_ID_AUDIO);
    registerCanCallback(primaryHandler);
    registerCanCallback(secondaryHandler);  // Both will be called
}
```

### Custom Message Types
```cpp
// Define custom message types (use values not in conflict)
#define MY_CUSTOM_MESSAGE 0x80

void sendCustomMessage() {
    uint8_t data[3] = {MY_CUSTOM_MESSAGE, param1, param2};
    sendCanMessage(CAN_ID_TARGET, data, 3);
}
```

### Connection Monitoring
```cpp
// Access connection status (defined in can_bus.cpp)
extern bool audioModuleConnected;
extern bool serialDisplayConnected;
extern unsigned long lastAudioPing;

void checkConnections() {
    if (audioModuleConnected) {
        Serial.print("Audio last seen: ");
        Serial.print((millis() - lastAudioPing) / 1000);
        Serial.println(" seconds ago");
    } else {
        Serial.println("Audio module disconnected");
    }
}
```

## Debugging

### Debug Output
The library provides debug output for troubleshooting:
```
CAN bus initialized
CAN ID: 0x262
Audio module connected (heartbeat)
Assigned ID: 2
```

### Common Debug Messages
```
// ID negotiation
Starting ID negotiation for moduleType=0x13
Assigned ID: 2

// Connection detection  
Audio module connected (direct message)
Serial display disconnected

// Errors
Timer module cannot negotiate IDs
CAN init failed - retrying
```

### Troubleshooting

#### Bus Initialization Fails
- Check SPI wiring to MCP2515
- Verify power supply to CAN controller
- Ensure correct crystal frequency (8MHz)
- Check CS pin definition matches hardware

#### Messages Not Received
- Verify CAN bus termination (120Ω at ends)
- Check for bus conflicts (multiple drivers)
- Ensure handleCanMessages() called in loop
- Verify message filtering isn't blocking messages

#### ID Negotiation Fails  
- Check for multiple modules starting simultaneously
- Verify assignUniqueId() called after initCanBus()
- Look for ID_PROBE/ID_TAKEN conflicts in output
- Ensure modules aren't using conflicting types

## Performance

### Message Throughput
- **Typical Load**: 10-15 messages/second during gameplay
- **Peak Load**: 30-50 messages/second during busy periods
- **Bus Capacity**: 500 kbps (thousands of messages/second theoretical)

### Memory Usage
- **Flash**: ~2KB for library code
- **RAM**: ~200 bytes for buffers and state
- **Per Callback**: ~4 bytes per registered callback

### Timing
- **Message Latency**: <1ms for high-priority messages
- **ID Negotiation**: 200-500ms depending on conflicts
- **Connection Timeout**: 5 seconds default

## Integration with Other Libraries

### Heartbeat Library
```cpp
#include <can_bus.h>
#include <heartbeat.h>

void setup() {
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    assignUniqueId(CAN_TYPE_SIMON);
    initHeartbeat(HEARTBEAT_INTERVAL_MODULE);  // Uses CAN bus automatically
}
```

### Module-Specific Libraries
```cpp
#include <can_bus.h>
#include <simon_says.h>  // Module library that depends on CAN bus

void setup() {
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    // Module library will register its own CAN callbacks
    simonSays.begin();
}
```

## Version History

- **v2.0**: Cleaned up debug output, improved connection detection
- **v1.5**: Added enhanced heartbeat support, standardized message format  
- **v1.0**: Initial implementation with basic ID negotiation

## See Also

- [Heartbeat Library](../heartbeat/) - Built on top of CAN bus
- [Shared Libraries Overview](../) - Complete library documentation
- [MCP2515 Datasheet](https://www.microchip.com/en-us/product/MCP2515) - CAN controller reference 