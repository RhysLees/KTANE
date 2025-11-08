# Module State Library

A shared library for KTANE modules that handles all module-to-timer communication, discovery, heartbeat management, status LED control, and bomb state data management.

## Overview

The `module_state` library provides a unified interface for all KTANE modules (except the timer) to:
- Automatically discover and register with the timer module
- Send periodic heartbeats with adaptive timing (1s discovery, 5s game)
- Control status LED (discovery flashing, strike flash, solved state)
- Manage game state (game running/stopped)
- Store and access bomb state data (serial number, edgework, strike count)
- Handle strike events with automatic LED flashing

## Features

- **Automatic Discovery**: Sends `MODULE_REGISTER` messages automatically
- **Adaptive Heartbeat Timing**: 1s during discovery, 5s during game
- **Status LED Control**: Automatic discovery flashing, strike flash, manual override
- **Game State Management**: Handles `TIMER_GAME_START`/`TIMER_GAME_STOP` messages
- **Bomb State Storage**: Stores serial number, edgework, strike count
- **Callbacks**: Optional callbacks for game state changes, strikes, serial number
- **CAN Helpers**: Convenience wrappers for sending CAN messages and triggering shared audio
- **Status Telemetry**: Automatically reports status/progress/solved state changes back to the timer
- **Simple API**: Easy integration with minimal code

## Quick Start

### Basic Usage

```cpp
#include <module_state.h>

void onGameStateChange(bool running) {
    // Called when game starts/stops
    Serial.println(running ? "Game started" : "Game stopped");
}

void onStrike(uint8_t strikes) {
    // Called when strike count changes
    Serial.print("Strikes: ");
    Serial.println(strikes);
}

void onSerialNumber(const String& serial) {
    // Called when serial number is received
    Serial.print("Serial: ");
    Serial.println(serial);
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    // Let module_state handle timer messages
    moduleStateHandleCanMessage(id, senderId, data, len);
    
    // Handle module-specific messages here
}

void setup() {
    Serial.begin(115200);
    
    // Initialize CAN bus
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    assignUniqueId(CAN_TYPE_SIMON);
    registerCanCallback(onCanMessage);
    
    // Initialize module state with status LED on pin 11
    initModuleState(11);
    
    // Set callbacks (optional)
    if (globalModuleState) {
        globalModuleState->setGameStateCallback(onGameStateChange);
        globalModuleState->setStrikeCallback(onStrike);
        globalModuleState->setSerialNumberCallback(onSerialNumber);
    }
}

void loop() {
    handleCanMessages();
    updateModuleState();
    
    // Update module state
    setModuleStateStatus(MODULE_STATUS_ACTIVE);
    setModuleStateProgress(60);

    // Play a Simon tone through the shared audio module
    if (shouldPlayRedTone) {
        globalModuleState->playAudio(AUDIO_SIMON_RED);
    }
    
    if (moduleSolved) {
        setModuleStateSolved(true);
    }
}
```

### Advanced Usage (Class-based)

```cpp
#include <module_state.h>

ModuleState moduleState;

void onGameStateChange(bool running) {
    // Handle game state change
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    moduleState.handleCanMessage(id, senderId, data, len);
}

void setup() {
    // Initialize CAN bus
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    assignUniqueId(CAN_TYPE_SIMON);
    registerCanCallback(onCanMessage);
    
    // Initialize module state
    moduleState.begin(11);  // Status LED on pin 11
    moduleState.setGameStateCallback(onGameStateChange);
    
    // Access serial number and edgework
    String serial = moduleState.getSerialNumber();
    uint8_t batteries = moduleState.getBatteryCount();
}

void loop() {
    moduleState.update();
    
    // Update status
    moduleState.setStatus(MODULE_STATUS_ACTIVE);
    moduleState.setProgress(75);
    
    if (solved) {
        moduleState.setSolved(true);
    }
}
```

## API Reference

### Initialization

- `void begin(int statusLedPin = MODULE_STATE_DEFAULT_LED_PIN)` - Initialize module state
- `void update()` - Update module state (call in loop())

### CAN Message Handling

- `void handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len)` - Process CAN messages from timer

### Status Management

- `void setStatus(ModuleStatus status)` - Set module status
- `void setProgress(uint8_t progressPercent)` - Set progress (0-100)
- `void setSolved(bool solved = true)` - Mark module as solved
- `ModuleStatus getStatus() const` - Get current status
- `uint8_t getProgress() const` - Get current progress
- `bool isSolved() const` - Check if solved

### Game State

- `bool isGameRunning() const` - Check if game is running
- `bool isDiscoveredByTimer() const` - Check if discovered by timer

### Bomb State Accessors

- `String getSerialNumber() const` - Get serial number
- `const Edgework& getEdgework() const` - Get edgework data
- `uint8_t getStrikeCount() const` - Get current strike count
- `uint8_t getBatteryCount() const` - Get battery count

### Callbacks

- `void setGameStateCallback(GameStateCallback cb)` - Set game state change callback
- `void setStrikeCallback(StrikeCallback cb)` - Set strike callback
- `void setSerialNumberCallback(SerialNumberCallback cb)` - Set serial number callback
- `void setEdgeworkCallback(EdgeworkCallback cb)` - Set edgework callback

### Manual Control

- `void sendRegisterNow()` - Send registration immediately
- `void sendHeartbeatNow()` - Send heartbeat immediately
- `void triggerStrike()` - Trigger a strike (send MODULE_STRIKE)
- `void setCommunicationEnabled(bool enabled)` - Enable/disable automatic CAN traffic
- `void setDiscovered(bool discovered = true)` - Manually set discovery state
- `void setGameRunning(bool running)` - Manually set game running state
- `void setStrikeCount(uint8_t strikeCount)` - Manually set strike count
- `void setSerialNumber(const String& serial)` - Manually set serial number
- `void setEdgework(const Edgework& edgework)` - Manually set full edgework data

### CAN Integration

- `uint16_t getModuleId() const` - Returns the module's current CAN ID (after negotiation)
- `uint8_t getModuleInstanceId() const` - Returns the negotiated instance ID
- `bool sendMessage(uint16_t receiverId, const uint8_t* data, uint8_t len)` - Send a CAN payload to any module (max 6 bytes)
- `bool sendTimerMessage(const uint8_t* data, uint8_t len)` - Convenience wrapper for messages to the timer
- `bool sendBroadcastMessage(const uint8_t* data, uint8_t len)` - Send a broadcast CAN message
- `bool playAudio(CanAudioSound sound)` - Trigger a single shared audio sound (e.g., `AUDIO_SIMON_RED`)
- `bool playAudio(const uint8_t* soundCodes, uint8_t len)` - Trigger up to 6 queued sounds in a single CAN frame
- `bool sendTelemetry(uint8_t telemetryType, const uint8_t* payload, uint8_t len)` - Send module-specific telemetry (packs into a `MODULE_STATUS` frame)

### LED Control

- `void setLedPin(int pin)` - Set status LED pin
- `void setLedState(bool state)` - Manually set LED state (override automatic)
- `void clearLedOverride()` - Return to automatic LED control
- `void disableLed()` - Disable LED updates and release the pin

### Global Convenience Functions

- `void initModuleState(int statusLedPin = MODULE_STATE_DEFAULT_LED_PIN)` - Initialize global instance
- `void updateModuleState()` - Update global instance
- `void setModuleStateStatus(ModuleStatus status)` - Set status
- `void setModuleStateProgress(uint8_t progress)` - Set progress
- `void setModuleStateSolved(bool solved)` - Set solved
- `void moduleStateHandleCanMessage(...)` - Handle CAN message
- `void setModuleStateLedPin(int pin)` - Set LED pin
- `void setModuleStateLedState(bool state)` - Set LED state
- `void clearModuleStateLedOverride()` - Clear LED override
- `void disableModuleStateLed()` - Disable LED handling for the global instance
- `void setModuleStateCommunicationEnabled(bool enabled)` - Enable/disable CAN traffic for the global instance
- `void setModuleStateDiscovered(bool discovered = true)` - Manually set discovery state
- `void setModuleStateGameRunning(bool running)` - Manually set game running state
- `void setModuleStateStrikeCount(uint8_t strikeCount)` - Manually set strike count
- `void setModuleStateSerialNumber(const String& serial)` - Manually set serial number
- `void setModuleStateEdgework(const Edgework& edgework)` - Manually set full edgework data

## Status LED Behavior

| State | LED Behavior |
|-------|-------------|
| **Not Discovered** | Flash 500ms ON / 500ms OFF |
| **Discovered, Game Not Running** | LED OFF |
| **Game Running** | LED OFF (normal) |
| **Strike Event** | Flash ON once (500ms) |
| **Module Solved** | LED ON (solid) - set via `setLedState(true)` |

## Module Status Types

- `MODULE_STATUS_IDLE` - Module is idle/waiting
- `MODULE_STATUS_ACTIVE` - Module is actively processing
- `MODULE_STATUS_SOLVED` - Module has been solved
- `MODULE_STATUS_ARMED` - Needy module is armed/active
- `MODULE_STATUS_ERROR` - Module encountered an error

## Edgework Access

The library stores edgework data received from the timer. Access it via:

```cpp
const Edgework& edgework = moduleState.getEdgework();

// Check indicators
if (edgework.hasLitIndicator(IndicatorType::FRK)) {
    // Handle FRK indicator
}

// Check ports
if (edgework.hasPort(PortType::PARALLEL)) {
    // Handle parallel port
}

// Get counts
uint8_t batteries = moduleState.getBatteryCount();
uint8_t litIndicators = edgework.getLitIndicatorCount();
uint8_t ports = edgework.getPortCount();
```

## Heartbeat Timing

- **Discovery Mode (1s)**: When not discovered or game not running
- **Game Mode (5s)**: When discovered and game running

The library automatically switches between modes based on game state.

## Migration from Heartbeat Library

### Old Code
```cpp
#include <heartbeat.h>

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    if ((id == CAN_ID_TIMER || id == CAN_ID_BROADCAST) && len >= 1) {
        uint8_t msgType = data[0];
        switch (msgType) {
            case TIMER_GAME_START:
                setHeartbeatGameRunning(true);
                break;
            case TIMER_SERIAL_NUMBER:
                // Parse serial number...
                break;
            // ... many more cases
        }
    }
}

void setup() {
    initHeartbeat();
    // Manual LED control...
    // Manual registration...
}
```

### New Code
```cpp
#include <module_state.h>

void onGameStateChange(bool running) {
    // Handle game state change
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    moduleStateHandleCanMessage(id, senderId, data, len);
    // Handle module-specific messages here
}

void setup() {
    initModuleState(STATUS_LED_PIN);
    if (globalModuleState) {
        globalModuleState->setGameStateCallback(onGameStateChange);
    }
}
```

## Notes

- **Timer Module Exception**: The timer module should NOT use this library
- **CAN Message Handling**: Must call `handleCanMessage()` from your module's CAN callback
- **Status LED**: Optional - set to `MODULE_STATE_NO_LED` (or call `disableLed`) to disable
- **Edgework**: Currently edgework is not sent from timer (future enhancement)
- **Automatic Timing**: Heartbeat timing is automatic based on discovery and game state
- **Offline Modules**: Disable communications and use the manual setters when running without CAN

## Configuring the Default LED Pin

- Define `MODULE_STATE_DEFAULT_LED_PIN` **before** including `module_state.h` to set a project-wide default
- Defaults to `MODULE_STATE_NO_LED` if not specified
- Individual modules can override by passing an explicit pin to `initModuleState()` or `setModuleStateLedPin()`

## See Also

- [CAN Bus Library](../can_bus/) - Underlying communication system
- [Game State](../../timer/lib/game_state/) - Timer-side module management
- [Shared Libraries Overview](../) - Complete library documentation

