# Heartbeat Library

A shared library for KTANE modules to send periodic heartbeats to the timer module for connection detection and status reporting with intelligent timing based on game state.

## Overview

The heartbeat library provides a standardized way for all KTANE modules (except the timer) to:
- Announce their presence to the timer module
- Report their current status and progress
- Enable reliable connection detection with adaptive timing
- Automatically switch between discovery and game modes

## Features

- **Adaptive Timing**: Automatically switches between fast discovery (1s) and game operation (5s)
- **Game State Awareness**: Responds to game start/stop messages from timer
- **Status Reporting**: Reports module state, solved status, and progress
- **Simple Integration**: Easy API with global convenience functions
- **Enhanced Messages**: Includes status and progress information in heartbeats

## Timing Modes

### Discovery Mode (1 second)
- **When**: Game is not running (default state)
- **Purpose**: Fast module discovery and registration
- **Benefit**: Quick detection of new modules connecting to system

### Game Mode (5 seconds) 
- **When**: Game is actively running
- **Purpose**: Regular health monitoring during gameplay
- **Benefit**: Reduces CAN bus traffic while maintaining reliable monitoring

## Quick Start

### Simple Usage
```cpp
#include <heartbeat.h>

void setup() {
    // Initialize heartbeat system (starts in discovery mode)
    initHeartbeat();
}

void loop() {
    // Update heartbeat system (call every loop)
    updateHeartbeat();
    
    // Update status as needed
    setHeartbeatStatus(MODULE_STATUS_ACTIVE);
    setHeartbeatProgress(75);  // 75% complete
}

void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle game state changes from timer
    if (id == CAN_ID_BROADCAST && len >= 1) {
        if (data[0] == TIMER_GAME_START) {
            setHeartbeatGameRunning(true);
        } else if (data[0] == TIMER_GAME_STOP) {
            setHeartbeatGameRunning(false);
        }
    }
}
```

### Advanced Usage (Class-based)
```cpp
#include <heartbeat.h>

HeartbeatManager heartbeat;

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

void onGameStart() {
    heartbeat.setGameRunning(true);  // Switches to 5s heartbeats
}
```

## API Reference

### Global Functions

- `initHeartbeat()` - Initialize with discovery mode timing
- `updateHeartbeat()` - Update heartbeat (call in loop)
- `setHeartbeatGameRunning(bool)` - Switch between discovery/game modes
- `setHeartbeatStatus(status)` - Set current module status
- `setHeartbeatProgress(percent)` - Set progress (0-100)
- `setHeartbeatSolved(solved)` - Mark module as solved/unsolved
- `sendHeartbeatNow()` - Send immediate heartbeat

### HeartbeatManager Class Methods

- `begin()` - Initialize the heartbeat system
- `update()` - Update heartbeat timing (call in loop)
- `setGameRunning(bool)` - Switch timing modes
- `getCurrentInterval()` - Get current heartbeat interval
- `setStatus(ModuleStatus)` - Set module status
- `setProgress(uint8_t)` - Set progress percentage
- `setSolved(bool)` - Mark as solved/unsolved
- `sendNow()` - Send immediate heartbeat

### Module Status Types

- `MODULE_STATUS_IDLE` - Module is idle/waiting
- `MODULE_STATUS_ACTIVE` - Module is actively processing
- `MODULE_STATUS_SOLVED` - Module has been solved
- `MODULE_STATUS_ARMED` - Needy module is armed/active
- `MODULE_STATUS_ERROR` - Module encountered an error

### Timing Constants

- `HEARTBEAT_INTERVAL_DISCOVERY` - 1000ms (1 second)
- `HEARTBEAT_INTERVAL_GAME` - 5000ms (5 seconds)

## Integration Examples

### Audio Module
```cpp
#include <heartbeat.h>

void setup() {
    initCanBus(CAN_ID_AUDIO);
    initHeartbeat();  // Starts in discovery mode
}

void handleAudioMessage() {
    setHeartbeatStatus(MODULE_STATUS_ACTIVE);
    // Process audio...
    setHeartbeatStatus(MODULE_STATUS_IDLE);
}

void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    // Handle game state changes
    if (id == CAN_ID_BROADCAST && len >= 1) {
        if (data[0] == TIMER_GAME_START) {
            setHeartbeatGameRunning(true);
            Serial.println("Switching to 5s heartbeats");
        } else if (data[0] == TIMER_GAME_STOP) {
            setHeartbeatGameRunning(false);
            Serial.println("Switching to 1s heartbeats");
        }
    }
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
    initHeartbeat();  // Starts in discovery mode
    
    // Register with timer
    uint8_t registerData[1] = {MODULE_REGISTER};
    sendCanMessage(CAN_ID_TIMER, registerData, 1);
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

void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    if (id == CAN_ID_BROADCAST && len >= 1) {
        if (data[0] == TIMER_GAME_START) {
            setHeartbeatGameRunning(true);
            gameActive = true;
        } else if (data[0] == TIMER_GAME_STOP) {
            setHeartbeatGameRunning(false);
            gameActive = false;
        }
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

The timer module automatically:
- **Tracks Discovery**: Monitors 1s heartbeats to discover available modules
- **Manages Registration**: Allows modules to register for games
- **Monitors Health**: Watches 5s heartbeats during gameplay
- **Error Detection**: Stops game if registered modules disconnect
- **Broadcasts State**: Sends game start/stop messages to switch timing modes

## Timing Behavior

### System Startup
1. **Module Powers On**: Starts sending 1s discovery heartbeats
2. **Timer Detects**: Module appears in discovery list
3. **Module Registers**: Sends MODULE_REGISTER message
4. **Timer Tracks**: Module moves to registered list

### Game Start
1. **Timer Broadcasts**: TIMER_GAME_START message
2. **Modules Switch**: Heartbeat interval changes to 5s
3. **Monitoring Active**: Timer watches for timeouts
4. **Error Detection**: Missing heartbeats stop game

### Game Stop
1. **Timer Broadcasts**: TIMER_GAME_STOP message  
2. **Modules Switch**: Heartbeat interval changes to 1s
3. **Discovery Active**: Fast discovery resumes
4. **Registration Open**: New modules can join

## Debugging

### Expected Output
```
// Module startup
Heartbeat: Initialized - Discovery mode (1000ms)

// Game state changes  
Heartbeat: Game state changed to RUNNING (5000ms)
Heartbeat: Game state changed to DISCOVERY (1000ms)

// Timer module discovery reports
Discovered modules (3): AUDIO, SIMON #1, SIMON #2
```

### Troubleshooting

#### Module Not Discovered
- Check 1s heartbeats are being sent
- Verify CAN bus connectivity
- Ensure timer module is running
- Check for CAN ID conflicts

#### Game Stops Unexpectedly
- Check 5s heartbeats during game
- Verify module didn't crash or disconnect
- Look for "CRITICAL ERROR" messages in timer output
- Check CAN bus stability under load

#### Heartbeat Timing Issues
- Verify `updateHeartbeat()` called in loop
- Check game state synchronization
- Ensure timer broadcasts reach modules
- Verify message parsing is correct

## Performance

### Discovery Mode (1s heartbeats)
- **Purpose**: Fast module detection
- **CAN Load**: ~3 messages/second per module
- **Discovery Time**: <2 seconds for new modules

### Game Mode (5s heartbeats)
- **Purpose**: Health monitoring
- **CAN Load**: <1 message/second per module  
- **Timeout Detection**: 10 seconds maximum

### Mode Switching
- **Trigger**: Timer game start/stop broadcasts
- **Response Time**: <100ms to switch modes
- **Reliability**: Immediate heartbeat sent on state change

## Migration from v1.x

### Old API (Multiple Intervals)
```cpp
// OLD - Multiple predefined intervals
initHeartbeat(HEARTBEAT_INTERVAL_AUDIO);      // 2s
initHeartbeat(HEARTBEAT_INTERVAL_MODULE);     // 5s
initHeartbeat(HEARTBEAT_INTERVAL_NEEDY);      // 1s
```

### New API (Adaptive Timing)
```cpp
// NEW - Single adaptive system
initHeartbeat();  // Starts at 1s, switches to 5s during game

// Add game state handling
void onCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    if (id == CAN_ID_BROADCAST && len >= 1) {
        if (data[0] == TIMER_GAME_START) {
            setHeartbeatGameRunning(true);
        } else if (data[0] == TIMER_GAME_STOP) {
            setHeartbeatGameRunning(false);
        }
    }
}
```

## Notes

- **Timer Module Exception**: The timer module should NOT use this library
- **Game State Required**: Modules must handle TIMER_GAME_START/STOP messages
- **CAN Format**: Uses simplified message format (command-first)
- **Automatic Timing**: No manual interval management needed
- **Error Recovery**: System automatically recovers from timing issues

## See Also

- [CAN Bus Library](../can_bus/) - Underlying communication system
- [Game State](../../timer/lib/game_state/) - Timer-side module management and game state
- [Shared Libraries Overview](../) - Complete library documentation 