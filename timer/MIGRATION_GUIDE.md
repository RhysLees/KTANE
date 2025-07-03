# KTANE Timer Libraries Migration Guide

## Overview

All timer libraries have been updated to work with the new Game State System v2.0. This guide explains the changes and how to migrate existing code.

## Summary of Changes

### 🎯 **Core Changes**
- **New Game State System**: Complete rewrite with proper KTANE mechanics
- **Parameter Changes**: All display update functions now require `GameStateManager&` parameter
- **Event-Driven Architecture**: Callbacks for state changes, strikes, module solving
- **Enhanced Module Management**: Proper module categories and needy module support

### 📁 **Updated Files**

#### **Game State System**
- ✅ `game_state_v2.h` - New comprehensive game state header
- ✅ `game_state_v2.cpp` - Complete implementation with KTANE mechanics
- ✅ `README.md` - Comprehensive documentation
- ✅ `example_integration.cpp` - Integration example

#### **Display Libraries**
- ✅ `countdown/countdown.h` - Updated API
- ✅ `countdown/countdown.cpp` - Game state integration
- ✅ `strikes/strikes.h` - Updated API  
- ✅ `strikes/strikes.cpp` - Game state integration

#### **Control Libraries**
- ✅ `serial_command/serial_command.h` - Updated API
- ✅ `serial_command/serial_command.cpp` - Enhanced commands
- ✅ `debug/debug.h` - Updated API
- ✅ `debug/debug.cpp` - Game state integration

#### **Utility Libraries**
- ✅ `module_tracker/module_tracker.h` - Updated references
- ✅ `module_tracker/module_tracker.cpp` - Game state integration

#### **Main Application**
- ✅ `src/main.cpp` - Updated to use new system
- ✅ `test_integration.cpp` - Test file for verification

## API Changes

### Before (v1.0)
```cpp
// Old game state
#include <game_state.h>
extern GameStateManager gameState;

// Old function calls
updateCountdownDisplay();
updateStrikeCount();
handleSerialCommands();
updateDebugInterface();

// Old enums
gameState.setState(GAME_RUNNING);
if (gameState.is(GAME_EXPLODED)) { ... }
```

### After (v2.0)
```cpp
// New game state
#include "game_state_v2.h"
GameStateManager gameState; // Local instance

// New function calls - require game state parameter
updateCountdownDisplay(gameState);
updateStrikeCount(gameState);
handleSerialCommands(gameState);
updateDebugInterface(gameState);

// New enum class
gameState.setState(GameState::RUNNING);
if (gameState.getState() == GameState::EXPLODED) { ... }
```

## Migration Steps

### 1. Update Includes
```cpp
// Replace old includes
- #include <game_state.h>
+ #include "game_state_v2.h"

// Remove external declarations
- extern GameStateManager gameState;
```

### 2. Update Function Calls
```cpp
// Add game state parameter to all display functions
- updateCountdownDisplay();
+ updateCountdownDisplay(gameState);

- updateStrikeCount();
+ updateStrikeCount(gameState);

- handleSerialCommands();
+ handleSerialCommands(gameState);

- updateDebugInterface();
+ updateDebugInterface(gameState);
```

### 3. Update Enum Usage
```cpp
// Update state enums
- GAME_IDLE → GameState::IDLE
- GAME_RUNNING → GameState::RUNNING  
- GAME_EXPLODED → GameState::EXPLODED
- GAME_SOLVED → GameState::DEFUSED

// Update method calls
- gameState.is(GAME_RUNNING)
+ gameState.getState() == GameState::RUNNING
```

### 4. Add Callbacks (Optional)
```cpp
// Set up event callbacks
gameState.setStateChangeCallback([](GameState oldState, GameState newState) {
    // Handle state changes
});

gameState.setStrikeChangeCallback([](uint8_t strikes) {
    // Handle strike changes
});

gameState.setModuleSolvedCallback([](uint8_t solved, uint8_t total) {
    // Handle module solving
});
```

### 5. Update Configuration
```cpp
// Configure game settings
GameConfig config;
config.timeLimitMs = 300000;  // 5 minutes
config.maxStrikes = 3;
config.enableStrikeAcceleration = true;
config.strikeAccelerationFactor = 0.25f;
config.enableEmergencyAlarm = true;
config.emergencyAlarmThreshold = 60000;
config.enableNeedyModules = true;
config.enableEdgework = true;

gameState.setConfig(config);
```

## New Features Available

### 1. Enhanced Module Management
```cpp
// Register modules with proper types
gameState.registerModule(0x10, ModuleType::WIRES);
gameState.registerModule(0x30, ModuleType::VENTING_GAS);

// Check module categories
uint8_t regular = gameState.getRegularModules();
uint8_t needy = gameState.getNeedyModules();
bool allSolved = gameState.allModulesSolved();
```

### 2. Comprehensive Edgework
```cpp
// Access edgework information
bool hasLitFRK = gameState.hasLitIndicator(IndicatorType::FRK);
bool hasParallel = gameState.hasPort(PortType::PARALLEL);
uint8_t batteries = gameState.getBatteryCount();
```

### 3. Enhanced Serial Commands
```
START           - Start/resume the game
STOP            - Pause the game  
RESET           - Reset game to initial state
TIME mm:ss      - Set countdown duration
STRIKE x        - Set strike count
MODULE id type  - Register a module
SOLVE id        - Mark module as solved
STATUS          - Show detailed game status
EDGEWORK        - Show edgework information
CONFIG          - Show configuration
```

### 4. Debug Interface Improvements
- Live game state display
- Module list viewer
- Edgework information viewer
- Enhanced menu system
- Hard reset functionality

## Testing

Use the provided test file to verify integration:

```bash
# Compile and upload test_integration.cpp to verify all libraries work
```

Expected output:
```
=== Testing Game State v2.0 ===
Initial state: 0
Serial number: AB3CD4
Total modules: 2
Needy modules: 1
...
All tests completed!
```

## Troubleshooting

### Common Issues

#### 1. Compilation Errors
```
Error: 'GAME_RUNNING' was not declared
```
**Solution**: Update enum usage to `GameState::RUNNING`

#### 2. Function Not Found
```
Error: 'updateCountdownDisplay' expects 1 argument
```
**Solution**: Add game state parameter: `updateCountdownDisplay(gameState)`

#### 3. Missing Include
```
Error: 'GameStateManager' was not declared
```
**Solution**: Include the new header: `#include "game_state_v2.h"`

### Performance Notes

- **Memory Usage**: ~2KB additional for game state management
- **CPU Usage**: Minimal overhead for embedded systems
- **Update Frequency**: Designed for 60Hz game loop

## Benefits of Migration

### ✅ **Improved Features**
- Complete KTANE game mechanics
- Proper module categorization
- Needy module support
- Enhanced edgework system
- Event-driven architecture

### ✅ **Better Debugging**
- Comprehensive status reporting
- Module tracking and timeout detection
- Enhanced serial command interface
- Debug LCD interface improvements

### ✅ **Future-Proof Design**
- Modular architecture
- Configurable game settings
- Extensible module types
- Statistics tracking

### ✅ **Maintainability**
- Clear separation of concerns
- Well-documented API
- Comprehensive error handling
- Type-safe enums

## Need Help?

- Check the comprehensive documentation in `README.md`
- Review the integration example in `example_integration.cpp`
- Test your changes with `test_integration.cpp`
- Use the enhanced serial commands for debugging

The migration provides significant improvements in functionality, maintainability, and adherence to the original KTANE game mechanics. 