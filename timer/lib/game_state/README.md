# KTANE Game State System v2.0

## Overview

This is a comprehensive game state management system for Keep Talking and Nobody Explodes (KTANE). It implements all the core game mechanics, module management, edgework, and proper game progression rules.

## Key Features

### 🎮 **Complete Game Mechanics**
- **6 Game States**: IDLE, RUNNING, PAUSED, EXPLODED, DEFUSED, VICTORY
- **Strike System**: Configurable max strikes with acceleration
- **Timer Management**: Strike-based speed acceleration, emergency alarms
- **Module Categories**: Regular, Needy, and Ignored modules
- **Edgework System**: Indicators, ports, and batteries
- **Serial Number Generation**: Proper KTANE format

### 🔧 **Advanced Module Management**
- **Dynamic Registration**: Modules can register/unregister during gameplay
- **Category Classification**: Automatic categorization based on module type
- **Needy Module Support**: Periodic activation with configurable intervals
- **Timeout Detection**: Automatic detection of disconnected modules
- **Solved State Tracking**: Proper victory condition checking

### ⚙️ **Configuration System**
- **Flexible Settings**: Time limits, strike counts, acceleration factors
- **Feature Toggles**: Enable/disable specific game mechanics
- **Runtime Configuration**: Change settings during gameplay

### 📊 **Statistics & Debugging**
- **Game Statistics**: Comprehensive tracking of game performance
- **Debug Interface**: Status printing and module inspection
- **Callback System**: Event-driven architecture for UI updates

## Game States

### `GameState::IDLE`
- **Description**: Waiting for game to start
- **Transitions**: Can start timer to enter RUNNING
- **Actions**: Generate serial number, setup edgework

### `GameState::RUNNING`
- **Description**: Active gameplay
- **Transitions**: Can pause, explode, or defuse
- **Actions**: Timer running, module solving, strike accumulation

### `GameState::PAUSED`
- **Description**: Game temporarily stopped
- **Transitions**: Can resume to RUNNING or reset to IDLE
- **Actions**: Timer stopped, modules inactive

### `GameState::EXPLODED`
- **Description**: Game over - bomb exploded
- **Cause**: Timer reached zero OR max strikes reached
- **Actions**: Stop timer, play explosion sound, end game

### `GameState::DEFUSED`
- **Description**: Victory - all modules solved
- **Cause**: All regular modules solved AND no active needy modules
- **Actions**: Stop timer, play defusal sound, victory sequence

### `GameState::VICTORY`
- **Description**: Game completed successfully
- **Transitions**: Can reset to start new game
- **Actions**: Statistics recording, victory celebration

## Module System

### Module Categories

#### **Regular Modules** (`ModuleCategory::REGULAR`)
- **Purpose**: Must be solved to win the game
- **Examples**: Wires, Button, Keypad, Simon Says, etc.
- **Behavior**: Can be solved once, count toward victory

#### **Needy Modules** (`ModuleCategory::NEEDY`)
- **Purpose**: Periodic challenges that don't count toward victory
- **Examples**: Venting Gas, Capacitor Discharge, Knob
- **Behavior**: Activate periodically, must be handled to prevent strikes

#### **Ignored Modules** (`ModuleCategory::IGNORED`)
- **Purpose**: Support modules that don't affect gameplay
- **Examples**: Timer, Audio, Serial Display, Edgework
- **Behavior**: Don't count toward victory or strikes

### Module Types

```cpp
// Regular Modules
WIRES = 0x10
BUTTON = 0x11
KEYPAD = 0x12
SIMON_SAYS = 0x13
WHOS_ON_FIRST = 0x14
MEMORY = 0x15
MORSE_CODE = 0x16
COMPLICATED_WIRES = 0x17
WIRE_SEQUENCES = 0x18
MAZE = 0x19
PASSWORD = 0x1A

// Needy Modules
VENTING_GAS = 0x30
CAPACITOR_DISCHARGE = 0x31
KNOB = 0x32

// Side Modules
SERIAL_DISPLAY = 0x40
INDICATOR_PANEL = 0x41
BATTERY_HOLDER = 0x42
PORT_PANEL = 0x43
```

## Edgework System

### Indicators
- **Types**: SND, CLR, CAR, IND, FRQ, SIG, NSA, MSA, TRN, BOB, FRK
- **States**: Lit or Unlit
- **Generation**: 0-3 random indicators per game
- **Usage**: Required for solving many modules

### Ports
- **Types**: PARALLEL, SERIAL, PS/2, RJ-45, RCA, DVI-D, STEREO-RCA
- **Generation**: 0-3 random ports per game
- **Usage**: Required for solving specific modules

### Batteries
- **Count**: 0-6 batteries
- **Generation**: Random count per game
- **Usage**: Required for solving many modules

## Timer System

### Strike Acceleration
- **Formula**: `speed = 1.0 + (accelerationFactor * strikeCount)`
- **Default**: 25% faster per strike
- **Configurable**: Can be disabled or adjusted

### Emergency Alarms
- **Threshold**: Configurable (default: 60 seconds)
- **Behavior**: Special audio alerts when time is low
- **Configurable**: Can be disabled

### Time Management
- **Pause/Resume**: Full pause functionality
- **Reset**: Reset to original time limit
- **Progress Tracking**: Percentage completion

## Usage Examples

### Basic Setup
```cpp
#include "game_state_v2.h"

// Create game state manager
GameStateManager gameState;

// Initialize
gameState.initialize();

// Set up callbacks
gameState.setStateChangeCallback([](GameState oldState, GameState newState) {
    Serial.print("State changed from ");
    Serial.print(static_cast<int>(oldState));
    Serial.print(" to ");
    Serial.println(static_cast<int>(newState));
});

gameState.setStrikeChangeCallback([](uint8_t strikes) {
    Serial.print("Strikes: ");
    Serial.println(strikes);
});
```

### Game Loop
```cpp
void loop() {
    // Update game state
    gameState.tick();
    
    // Handle game state
    switch (gameState.getState()) {
        case GameState::RUNNING:
            // Update displays, handle input
            break;
        case GameState::EXPLODED:
            // Show explosion, play sound
            break;
        case GameState::DEFUSED:
            // Show victory, play fanfare
            break;
    }
}
```

### Module Registration
```cpp
// Register a regular module
gameState.registerModule(0x10, ModuleType::WIRES);

// Register a needy module
gameState.registerModule(0x30, ModuleType::VENTING_GAS);

// Mark module as solved
gameState.setModuleSolved(0x10);
```

### Configuration
```cpp
GameConfig config;
config.timeLimitMs = 300000;  // 5 minutes
config.maxStrikes = 3;
config.enableStrikeAcceleration = true;
config.strikeAccelerationFactor = 0.25f;
config.enableEmergencyAlarm = true;
config.emergencyAlarmThreshold = 60000;

gameState.setConfig(config);
```

### Edgework Queries
```cpp
// Check indicators
if (gameState.hasLitIndicator(IndicatorType::FRK)) {
    // Handle FRK indicator logic
}

// Check ports
if (gameState.hasPort(PortType::PARALLEL)) {
    // Handle parallel port logic
}

// Check batteries
uint8_t batteries = gameState.getBatteryCount();
```

## Serial Commands

The system supports these serial commands:

```
START         - Start/resume the game
STOP          - Pause the game
RESET         - Reset to initial state
TIME mm:ss    - Set time limit
STRIKE x      - Set strike count
MODULE id type - Register a module
SOLVE id      - Mark module as solved
EDGEWORK      - Show edgework information
STATUS        - Show game status
CONFIG        - Show configuration
```

## Integration with Existing Code

### Migration from v1
1. Replace `#include <game_state.h>` with `#include "game_state_v2.h"`
2. Update enum usage: `GAME_RUNNING` → `GameState::RUNNING`
3. Update method calls to use new API
4. Add callback setup for state changes

### CAN Bus Integration
- Module registration via CAN messages
- Module solved notifications
- Needy module activation
- Audio command coordination

### Display Integration
- Timer display updates via callbacks
- Strike display updates via callbacks
- Status display via debug methods

## Advanced Features

### Statistics Tracking
```cpp
GameStateManager::GameStats stats = gameState.getStats();
Serial.print("Total game time: ");
Serial.println(stats.totalGameTime);
Serial.print("Modules solved: ");
Serial.println(stats.modulesSolved);
Serial.print("Was victory: ");
Serial.println(stats.wasVictory ? "YES" : "NO");
```

### Debug Interface
```cpp
// Print current status
gameState.printStatus();

// Print module information
gameState.printModules();

// Print edgework information
gameState.printEdgework();
```

### Custom Callbacks
```cpp
// State change callback
gameState.setStateChangeCallback([](GameState oldState, GameState newState) {
    // Handle state transitions
});

// Strike change callback
gameState.setStrikeChangeCallback([](uint8_t strikes) {
    // Update strike display
});

// Module solved callback
gameState.setModuleSolvedCallback([](uint8_t solved, uint8_t total) {
    // Update module counter
});

// Time update callback
gameState.setTimeUpdateCallback([](unsigned long remainingMs) {
    // Update timer display
});
```

## Performance Considerations

- **Memory Usage**: ~2KB for typical game configuration
- **CPU Usage**: Minimal overhead, optimized for embedded systems
- **Module Limit**: No hard limit, limited by available memory
- **Update Frequency**: Designed for 60Hz game loop

## Error Handling

- **Invalid Module IDs**: Gracefully ignored
- **Duplicate Registration**: Safely handled
- **Invalid State Transitions**: Logged and ignored
- **Memory Allocation**: Uses static allocation where possible

This system provides a complete, robust foundation for implementing the full KTANE experience with proper game mechanics, module management, and edgework integration. 