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

### 🖥️ [KTANE Console](ktane_console/) - Default debug `Stream`
**Purpose**: Single macro `KTANE_CONSOLE_OUT` (`Serial` or `Serial1`) so libraries and helpers can log without hard-coding USB vs UART.  
**Used by**: Audio stack (`sd_card`, `amp`, `audio_mixer`); any module can include it.  
**Key features**:
- Default: `KTANE_CONSOLE_OUT` → `Serial` (USB CDC on typical Pico W builds).
- With `-DKTANE_PICOPROBE_UART` in `build_flags`: → `Serial1` on GP0/GP1 (Pico Probe UART).

**Quick start**: Add `lib_extra_dirs = ../shared_libs` (if not already), then `#include <ktane_console.h>`.

### ⚙️ [Module State](module_state/) - State & Heartbeat Management
**Purpose**: Combines heartbeat, registration, and per-module state management  
**Used by**: All modules except timer  
**Key Features**:
- Automatic registration and heartbeats
- Status, progress, and solved reporting
- Centralised timer message handling
- Optional status LED management

**Quick Start**:
```cpp
#include <module_state.h>

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    moduleStateHandleCanMessage(id, senderId, data, len);
}

void setup() {
    initModuleState(MODULE_STATE_NO_LED);
}

void loop() {
    updateModuleState();
}
```

## 🏗️ Library Architecture

### Inter-Library Dependencies
```
┌──────────────┐
│ Module State │──┐
└──────────────┘  │
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
    initModuleState(MODULE_STATE_NO_LED);        // Audio module has no LED
}
```

#### Dynamic ID Modules (Game Modules)
```cpp
void setup() {
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));  // Temporary ID
    assignUniqueId(CAN_TYPE_SIMON);                      // Negotiate unique ID
    initModuleState(STATUS_LED_PIN);                     // Handles status + heartbeat
}
```

#### Timer Module (Special Case)
```cpp
void setup() {
    initCanBus(CAN_ID_TIMER);                    // Fixed timer ID
    // Note: Timer does NOT use module_state
}
```

## 🔧 Integration Guide

### Adding Libraries to PlatformIO Projects

Add to your module's `platformio.ini`:
```ini
[env:your_module]
lib_deps = 
    ../shared_libs/can_bus
    ../shared_libs/module_state
```

### Include Order
Always include CAN bus before module-level helpers:
```cpp
#include <Arduino.h>
#include <can_bus.h>        // Always first
#include <module_state.h>   // Uses CAN helpers
// ... other includes
```

### Standard Module Setup Pattern
```cpp
#include <Arduino.h>
#include <can_bus.h>
#include <module_state.h>

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    moduleStateHandleCanMessage(id, senderId, data, len);
    // Handle additional module-specific messages here
}

void setup() {
    Serial.begin(115200);
    
    // Initialize CAN bus (adjust for module type)
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_YOUR_MODULE, 0x00));
    registerCanCallback(onCanMessage);
    
    // For dynamic modules only
    assignUniqueId(CAN_TYPE_YOUR_MODULE);
    
    // Initialize module state (handles registration + heartbeat)
    initModuleState(STATUS_LED_PIN);
}

void loop() {
    handleCanMessages();    // Process CAN messages
    updateModuleState();    // Registration, heartbeats, LED, etc.
    
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
2. Verify module initializes state manager: `initModuleState()` called
3. Check for ID conflicts in serial output
4. Ensure `updateModuleState()` called in loop

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
ModuleState: Heartbeat interval set to 5000ms
Audio module connected (heartbeat)

// ID negotiation
Assigned ID: 2
CAN ID: 0x262

// Connection timeout
Audio module disconnected
```

## 📈 Performance Considerations

### Heartbeat Intervals
- **Audio**: 2s (frequent due to real-time audio needs)*
- **Serial Display**: 3s (moderate update frequency)*  
- **Game Modules**: 5s (standard for gameplay modules)
- **Needy Modules**: 1s (high frequency for urgent modules)

`*` Modules using `module_state` default to discovery (1s) until the game starts, then 5s.

### CAN Bus Load
- **Standard Game**: ~10-15 messages/second
- **Heavy Activity**: ~30-50 messages/second  
- **Bus Capacity**: 500 kbps (thousands of messages/second)

### Memory Usage
- **CAN Bus Library**: ~2KB flash, ~200 bytes RAM
- **Module State Library**: ~3KB flash, ~200 bytes RAM
- **Combined Overhead**: <2% of typical microcontroller resources

## 🚀 Best Practices

### Module Development
1. **Always include CAN bus first** in includes
2. **Use module_state** instead of custom heartbeat implementations
3. **Follow standard setup pattern** for consistency
4. **Handle all expected message types** in CAN callback
5. **Keep module_state status flags** in sync with gameplay

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
    setModuleStateStatus(MODULE_STATUS_ACTIVE);
    setModuleStateProgress(calculateProgress());
} else if (solved) {
    setModuleStateSolved(true);
} else {
    setModuleStateStatus(MODULE_STATUS_IDLE);
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
- [Main Project README](../README.md)
- [Build Instructions](../BUILD_README.md)

---

*This documentation covers KTANE shared libraries v2.0. For module-specific documentation, see individual module directories.* 