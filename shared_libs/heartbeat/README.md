# Heartbeat Library

A shared library for KTANE modules to send periodic heartbeats to the timer module for connection detection and status reporting.

## Overview

The heartbeat library provides a standardized way for all KTANE modules (except the timer) to:
- Announce their presence to the timer module
- Report their current status and progress
- Enable reliable connection detection
- Send enhanced status information

## Features

- **Automatic Timing**: Handles heartbeat intervals automatically
- **Status Reporting**: Reports module state, solved status, and progress
- **Configurable Intervals**: Different intervals for different module types
- **Easy Integration**: Simple API with global convenience functions
- **Enhanced Messages**: Includes status and progress information in heartbeats

## Quick Start

### Simple Usage (Global Functions)

```cpp
#include <heartbeat.h>

void setup() {
    // Initialize with default 5-second interval
    initHeartbeat();
    
    // Or specify custom interval
    initHeartbeat(HEARTBEAT_INTERVAL_AUDIO);  // 2 seconds for audio
}

void loop() {
    // Update heartbeat system (call every loop)
    updateHeartbeat();
    
    // Update status as needed
    setHeartbeatStatus(MODULE_STATUS_ACTIVE);
    setHeartbeatProgress(75);  // 75% complete
}
```

### Advanced Usage (Class-based)

```cpp
#include <heartbeat.h>

HeartbeatManager heartbeat(3000);  // 3-second interval

void setup() {
    heartbeat.begin();
    heartbeat.setStatus(MODULE_STATUS_IDLE);
}

void loop() {
    heartbeat.update();
    
    // Update based on module state
    if (processing) {
        heartbeat.setStatus(MODULE_STATUS_ACTIVE);
        heartbeat.setProgress(calculateProgress());
    }
}
```

## API Reference

### Global Functions

- `initHeartbeat(intervalMs)` - Initialize with specified interval
- `updateHeartbeat()` - Update heartbeat (call in loop)
- `setHeartbeatStatus(status)` - Set current module status
- `setHeartbeatProgress(percent)` - Set progress (0-100)
- `setHeartbeatSolved(solved)` - Mark module as solved/unsolved
- `sendHeartbeatNow()` - Send immediate heartbeat

### Module Status Types

- `MODULE_STATUS_IDLE` - Module is idle/waiting
- `MODULE_STATUS_ACTIVE` - Module is actively processing
- `MODULE_STATUS_SOLVED` - Module has been solved
- `MODULE_STATUS_ARMED` - Needy module is armed/active
- `MODULE_STATUS_ERROR` - Module encountered an error

### Default Intervals

- `HEARTBEAT_INTERVAL_AUDIO` - 2000ms (2 seconds)
- `HEARTBEAT_INTERVAL_SERIAL_DISPLAY` - 3000ms (3 seconds)
- `HEARTBEAT_INTERVAL_MODULE` - 5000ms (5 seconds) - Default for game modules
- `HEARTBEAT_INTERVAL_NEEDY` - 1000ms (1 second) - For needy modules

## Integration Examples

### Audio Module
```cpp
#include <heartbeat.h>

void setup() {
    initCanBus(CAN_ID_AUDIO);
    initHeartbeat(HEARTBEAT_INTERVAL_AUDIO);
}

void handleAudioMessage() {
    setHeartbeatStatus(MODULE_STATUS_ACTIVE);
    // Process audio...
    setHeartbeatStatus(MODULE_STATUS_IDLE);
}

void loop() {
    handleCanMessages();
    updateHeartbeat();
}
```

### Game Module (Simon Says)
```cpp
#include <heartbeat.h>

void setup() {
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
    assignUniqueId(CAN_TYPE_SIMON);
    initHeartbeat(HEARTBEAT_INTERVAL_MODULE);
}

void updateGameState() {
    switch (gameState) {
        case IDLE:
            setHeartbeatStatus(MODULE_STATUS_IDLE);
            break;
        case PLAYING:
            setHeartbeatStatus(MODULE_STATUS_ACTIVE);
            setHeartbeatProgress((stage * 100) / maxStages);
            break;
        case SOLVED:
            setHeartbeatSolved(true);
            break;
    }
}

void loop() {
    updateGame();
    updateGameState();
    updateHeartbeat();
}
```

## Message Format

Heartbeats are sent as CAN messages with the following format:
```
[MODULE_HEARTBEAT, status, solved_flag, progress]
```

- **MODULE_HEARTBEAT** (0x24) - Message type identifier
- **status** - ModuleStatus enum value
- **solved_flag** - 1 if solved, 0 if not
- **progress** - Progress percentage (0-100)

## Timer Integration

The timer module automatically receives and processes heartbeats for:
- Connection detection and timeout handling
- Module status tracking
- Progress monitoring
- Game state management

No special configuration needed on the timer side - it automatically handles heartbeat messages from all modules using this library.

## Migration Guide

### From Custom Heartbeat Code

Replace custom heartbeat implementations:

**Before:**
```cpp
unsigned long lastHeartbeat = 0;
const unsigned long HEARTBEAT_INTERVAL = 2000;

void sendHeartbeat() {
    if (millis() - lastHeartbeat >= HEARTBEAT_INTERVAL) {
        uint8_t data[1] = {MODULE_HEARTBEAT};
        sendCanMessage(CAN_ID_TIMER, data, 1);
        lastHeartbeat = millis();
    }
}
```

**After:**
```cpp
#include <heartbeat.h>

void setup() {
    initHeartbeat(2000);
}

void loop() {
    updateHeartbeat();
}
```

## Notes

- **Timer Module Exception**: The timer module should NOT use this library as it's the heartbeat receiver
- **Automatic Status**: The library automatically manages timing and message formatting
- **Enhanced Data**: Provides richer status information than basic heartbeats
- **Connection Detection**: Enables reliable module connection detection in the timer 