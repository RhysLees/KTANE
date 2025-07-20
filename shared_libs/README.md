# KTANE Shared Libraries

This directory contains shared libraries used across multiple KTANE modules to provide common functionality and ensure consistency.

## 📚 Available Libraries

### 🚌 [CAN Bus](can_bus/) - Core Communication System
**Purpose**: Handles all CAN bus communication between KTANE modules  
**Used by**: All modules  
**Key Features**:
- Module ID management and negotiation
- Message routing and filtering  
- Connection detection and timeouts
- Standardized message formats

**Quick Start**:
```cpp
#include <can_bus.h>

void setup() {
    initCanBus(CAN_ID_AUDIO);
    registerCanCallback(onCanMessage);
}

void loop() {
    handleCanMessages();
}
```

### 💓 [Heartbeat](heartbeat/) - Connection Management
**Purpose**: Provides standardized heartbeat functionality for module health monitoring  
**Used by**: All modules except timer  
**Key Features**:
- Automatic heartbeat timing
- Status and progress reporting
- Connection detection support
- Configurable intervals per module type

**Quick Start**:
```cpp
#include <heartbeat.h>

void setup() {
    initHeartbeat(HEARTBEAT_INTERVAL_MODULE);
}

void loop() {
    updateHeartbeat();
    setHeartbeatStatus(MODULE_STATUS_ACTIVE);
}
```

## 🏗️ Library Architecture

### Inter-Library Dependencies
```
┌─────────────┐
│  Heartbeat  │──┐
└─────────────┘  │
                 ▼
┌─────────────┐
│   CAN Bus   │
└─────────────┘
```

**CAN Bus** is the foundation library that all other communication libraries depend on.

### Module Integration Patterns

#### Fixed ID Modules (Audio, Serial Display)
```cpp
void setup() {
    initCanBus(CAN_ID_AUDIO);                    // Fixed ID
    initHeartbeat(HEARTBEAT_INTERVAL_AUDIO);     // 2 second heartbeat
}
```

#### Dynamic ID Modules (Game Modules)
```cpp
void setup() {
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));  // Temporary ID
    assignUniqueId(CAN_TYPE_SIMON);                      // Negotiate unique ID
    initHeartbeat(HEARTBEAT_INTERVAL_MODULE);            // 5 second heartbeat
}
```

#### Timer Module (Special Case)
```cpp
void setup() {
    initCanBus(CAN_ID_TIMER);                    // Fixed timer ID
    // Note: Timer does NOT use heartbeat library
}
```

## 🔧 Integration Guide

### Adding Libraries to PlatformIO Projects

Add to your module's `platformio.ini`:
```ini
[env:your_module]
lib_deps = 
    ../shared_libs/can_bus
    ../shared_libs/heartbeat
```

### Include Order
Always include CAN bus before other communication libraries:
```cpp
#include <Arduino.h>
#include <can_bus.h>        // Always first
#include <heartbeat.h>      // After CAN bus
// ... other includes
```

### Standard Module Setup Pattern
```cpp
#include <Arduino.h>
#include <can_bus.h>
#include <heartbeat.h>

void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle incoming CAN messages
}

void setup() {
    Serial.begin(115200);
    
    // Initialize CAN bus (adjust for module type)
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_YOUR_MODULE, 0x00));
    registerCanCallback(onCanMessage);
    
    // For dynamic modules only
    assignUniqueId(CAN_TYPE_YOUR_MODULE);
    
    // Initialize heartbeat
    initHeartbeat(HEARTBEAT_INTERVAL_MODULE);
    
    // Register with timer
    uint8_t registerData[1] = {MODULE_REGISTER};
    sendCanMessage(CAN_ID_TIMER, registerData, 1);
}

void loop() {
    handleCanMessages();    // Process CAN messages
    updateHeartbeat();      // Send heartbeats
    
    // Your module logic here
}
```

## 📋 Message Types and IDs

### CAN ID Structure
```
11-bit CAN ID: MMMMMM IIIII
               |      |
               |      └─ Instance ID (0-31)
               └──────── Module Type (0-63)
```

### Common Message Types

#### Timer → Module Messages
- `TIMER_GAME_START` (0x10) - Game started
- `TIMER_GAME_STOP` (0x11) - Game stopped
- `TIMER_STRIKE_UPDATE` (0x12) - Strike count changed
- `TIMER_SERIAL_NUMBER` (0x13) - Serial number update
- `TIMER_RESET` (0x14) - Reset module
- `TIMER_TIME_UPDATE` (0x15) - Time remaining
- `TIMER_COUNTDOWN` (0x16) - Initialization countdown

#### Module → Timer Messages  
- `MODULE_REGISTER` (0x20) - Register with timer
- `MODULE_SOLVED` (0x21) - Module was solved
- `MODULE_STRIKE` (0x22) - Module caused a strike
- `MODULE_STATUS` (0x23) - Status update
- `MODULE_HEARTBEAT` (0x24) - Periodic heartbeat

#### ID Negotiation
- `ID_PROBE` (0x01) - Check if ID is available
- `ID_TAKEN` (0x02) - ID is already in use

### Module Types
```cpp
#define CAN_TYPE_TIMER   0x00    // Timer module
#define CAN_TYPE_AUDIO   0x01    // Audio system
#define CAN_TYPE_SIMON   0x13    // Simon Says
// ... see can_bus.h for complete list
```

## 🔍 Debugging and Troubleshooting

### CAN Bus Debug Commands
Available in timer module serial console:
- `CAN` - Show detailed CAN bus status and connections
- `STATUS` - Show game state and module information  
- `HELP` - List all available commands

### Common Issues

#### Module Not Detected
1. Check CAN wiring and termination
2. Verify module sends heartbeats: `initHeartbeat()` called
3. Check for ID conflicts in serial output
4. Ensure `updateHeartbeat()` called in loop

#### Message Not Received  
1. Verify correct CAN ID usage
2. Check message format matches expected structure
3. Ensure receiver has `handleCanMessages()` in loop
4. Check CAN bus initialization order

#### ID Negotiation Fails
1. Check for multiple modules of same type starting simultaneously
2. Verify `assignUniqueId()` called after `initCanBus()`
3. Look for `ID_PROBE`/`ID_TAKEN` messages in debug output

### Debug Output Examples
```
// Successful heartbeat
Heartbeat: Initialized with 5000ms interval
Audio module connected (heartbeat)

// ID negotiation
Assigned ID: 2
CAN ID: 0x262

// Connection timeout
Audio module disconnected
```

## 📈 Performance Considerations

### Heartbeat Intervals
- **Audio**: 2s (frequent due to real-time audio needs)
- **Serial Display**: 3s (moderate update frequency)  
- **Game Modules**: 5s (standard for gameplay modules)
- **Needy Modules**: 1s (high frequency for urgent modules)

### CAN Bus Load
- **Standard Game**: ~10-15 messages/second
- **Heavy Activity**: ~30-50 messages/second  
- **Bus Capacity**: 500 kbps (thousands of messages/second)

### Memory Usage
- **CAN Bus Library**: ~2KB flash, ~200 bytes RAM
- **Heartbeat Library**: ~1KB flash, ~50 bytes RAM
- **Combined Overhead**: <1% of typical microcontroller resources

## 🚀 Best Practices

### Module Development
1. **Always include CAN bus first** in includes
2. **Use heartbeat library** instead of custom implementations
3. **Follow standard setup pattern** for consistency
4. **Handle all expected message types** in CAN callback
5. **Update heartbeat status** to reflect module state

### Error Handling
```cpp
void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    if (len < 3) return;  // Validate message length
    
    uint8_t msgType = data[2];
    switch (msgType) {
        case TIMER_GAME_START:
            // Handle message
            break;
        default:
            // Log unknown message types for debugging
            Serial.print("Unknown message: 0x");
            Serial.println(msgType, HEX);
            break;
    }
}
```

### Status Reporting
```cpp
// Update status based on module activity
if (processing) {
    setHeartbeatStatus(MODULE_STATUS_ACTIVE);
    setHeartbeatProgress(calculateProgress());
} else if (solved) {
    setHeartbeatSolved(true);
} else {
    setHeartbeatStatus(MODULE_STATUS_IDLE);
}
```

## 📝 Adding New Libraries

When creating new shared libraries:

1. **Create directory**: `shared_libs/your_library/`
2. **Add files**: `your_library.h`, `your_library.cpp`, `README.md`
3. **Document API**: Include usage examples and integration guide
4. **Update this README**: Add library to the list above
5. **Test integration**: Verify with multiple module types

### Library Template Structure
```
shared_libs/your_library/
├── your_library.h        # Public API declarations
├── your_library.cpp      # Implementation
├── README.md            # Library-specific documentation
└── examples/            # Usage examples (optional)
```

## 🔗 See Also

- [CAN Bus Library Documentation](can_bus/)
- [Heartbeat Library Documentation](heartbeat/)
- [Main Project README](../README.md)
- [Build Instructions](../BUILD_README.md)

---

*This documentation covers KTANE shared libraries v2.0. For module-specific documentation, see individual module directories.* 