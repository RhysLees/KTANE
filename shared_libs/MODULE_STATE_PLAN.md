# Module State Library - Design Plan

## Overview

Replace the current `heartbeat` library with a new `module_state` shared library that handles all module-level game state management, connectivity, and status LED control. This will eliminate code duplication across modules and provide a unified interface for all module-to-timer communication.

## Current Architecture Analysis

### Current State
- **Heartbeat Library**: Handles periodic heartbeats and discovery, but modules must:
  - Manually send `MODULE_REGISTER` messages
  - Manually handle CAN message parsing for game state
  - Manually manage status LED flashing
  - Manually track serial number and edgework
  - Manually handle strike events

### Problems
- **Code Duplication**: Each module (Simon Says, etc.) copies the same CAN message handling
- **Inconsistent Status LED**: Each module implements discovery LED flashing differently
- **Missing State Management**: Modules don't have a unified way to access serial number/edgework
- **Strike Handling**: Modules must manually handle strike events

## Proposed Architecture

### New `module_state` Library

**Location**: `shared_libs/module_state/`

**Purpose**: Centralized module-to-timer communication and state management

### Core Responsibilities

1. **Discovery & Registration**
   - Automatically send `MODULE_REGISTER` on initialization
   - Handle `TIMER_MODULE_DISCOVERED` acknowledgment
   - Track discovery state

2. **Heartbeat Management**
   - Send periodic `MODULE_HEARTBEAT` messages
   - Adaptive timing: 1s (discovery) vs 5s (game)
   - Include status, progress, solved state

3. **Status LED Control**
   - Discovery mode: Flash 500ms on / 500ms off until discovered
   - After discovery: LED off (normal state)
   - Strike event: Flash red once (500ms)
   - Solved state: LED solid on (optional)

4. **Game State Management**
   - Receive and handle `TIMER_GAME_START` / `TIMER_GAME_STOP`
   - Track game running state
   - Notify module via callback

5. **Bomb State Data**
   - Receive and store serial number (`TIMER_SERIAL_NUMBER`)
   - Receive and store edgework (indicators, ports, batteries)
   - Provide accessor methods for modules

6. **Strike Handling**
   - Receive `TIMER_STRIKE_UPDATE` messages
   - Flash status LED red once when module should strike
   - Provide callback for strike events

## API Design

### Header File: `module_state.h`

```cpp
#pragma once

#include <Arduino.h>
#include <can_bus.h>
#include <stdint.h>

// Status LED pin (must be set by module)
// Default: -1 (no LED, must be set explicitly)
#define MODULE_STATE_NO_LED -1

// Module status flags
enum ModuleStatus : uint8_t {
    MODULE_STATUS_IDLE = 0x00,
    MODULE_STATUS_ACTIVE = 0x01,
    MODULE_STATUS_SOLVED = 0x02,
    MODULE_STATUS_ARMED = 0x03,      // For needy modules
    MODULE_STATUS_ERROR = 0xFF
};

// Edgework data structures (matching game_state.h)
enum class IndicatorType : uint8_t {
    SND = 0, CLR = 1, CAR = 2, IND = 3, FRQ = 4,
    SIG = 5, NSA = 6, MSA = 7, TRN = 8, BOB = 9, FRK = 10
};

enum class PortType : uint8_t {
    PARALLEL = 0, SERIAL_PORT = 1, PS2 = 2, RJ45 = 3,
    RCA = 4, DVI = 5, STEREO_RCA = 6
};

struct Indicator {
    IndicatorType type;
    bool lit;
    String label;
};

struct Port {
    PortType type;
    String label;
};

struct Edgework {
    std::vector<Indicator> indicators;
    std::vector<Port> ports;
    uint8_t batteryCount = 0;
    
    // Helper methods
    bool hasIndicator(IndicatorType type) const;
    bool hasLitIndicator(IndicatorType type) const;
    bool hasUnlitIndicator(IndicatorType type) const;
    bool hasPort(PortType type) const;
    uint8_t getLitIndicatorCount() const;
    uint8_t getUnlitIndicatorCount() const;
    uint8_t getPortCount() const;
};

// Callback types
typedef void (*GameStateCallback)(bool gameRunning);
typedef void (*StrikeCallback)(uint8_t strikeCount);
typedef void (*SerialNumberCallback)(const String& serial);
typedef void (*EdgeworkCallback)(const Edgework& edgework);

// Main class
class ModuleState {
private:
    // Discovery state
    bool isDiscovered;
    bool isRegistered;
    unsigned long lastRegisterAttempt;
    unsigned long lastDiscoveryFlashTime;
    
    // Heartbeat state
    unsigned long lastHeartbeat;
    bool gameRunning;
    ModuleStatus currentStatus;
    uint8_t progress;
    bool solved;
    
    // Status LED
    int statusLedPin;
    bool discoveryLedState;
    
    // Strike handling
    uint8_t currentStrikes;
    bool strikeFlashActive;
    unsigned long strikeFlashStart;
    
    // Bomb state
    String serialNumber;
    Edgework edgework;
    bool edgeworkReceived;
    
    // Callbacks
    GameStateCallback gameStateCallback;
    StrikeCallback strikeCallback;
    SerialNumberCallback serialNumberCallback;
    EdgeworkCallback edgeworkCallback;
    
    // Internal methods
    void handleTimerMessage(uint8_t msgType, const uint8_t* data, uint8_t len);
    void updateDiscoveryLed();
    void updateStrikeLed();
    void sendRegister();
    void sendHeartbeat();
    
public:
    ModuleState();
    
    // Initialization
    void begin(int statusLedPin = MODULE_STATE_NO_LED);
    void update();  // Call in loop()
    
    // CAN message handler (must be called from module's onCanMessage)
    void handleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);
    
    // Status management
    void setStatus(ModuleStatus status);
    void setProgress(uint8_t progressPercent);
    void setSolved(bool solved = true);
    ModuleStatus getStatus() const { return currentStatus; }
    uint8_t getProgress() const { return progress; }
    bool isSolved() const { return solved; }
    
    // Game state
    bool isGameRunning() const { return gameRunning; }
    bool isDiscoveredByTimer() const { return isDiscovered; }
    
    // Bomb state accessors
    String getSerialNumber() const { return serialNumber; }
    const Edgework& getEdgework() const { return edgework; }
    uint8_t getStrikeCount() const { return currentStrikes; }
    uint8_t getBatteryCount() const { return edgework.batteryCount; }
    
    // Callbacks
    void setGameStateCallback(GameStateCallback cb) { gameStateCallback = cb; }
    void setStrikeCallback(StrikeCallback cb) { strikeCallback = cb; }
    void setSerialNumberCallback(SerialNumberCallback cb) { serialNumberCallback = cb; }
    void setEdgeworkCallback(EdgeworkCallback cb) { edgeworkCallback = cb; }
    
    // Manual control
    void sendRegisterNow();
    void sendHeartbeatNow();
    void triggerStrike();  // For module-initiated strikes
};

// Global convenience functions (singleton pattern)
void initModuleState(int statusLedPin = MODULE_STATE_NO_LED);
void updateModuleState();
void setModuleStateStatus(ModuleStatus status);
void setModuleStateProgress(uint8_t progress);
void setModuleStateSolved(bool solved);
void moduleStateHandleCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len);

// Status LED control
void setModuleStateLedPin(int pin);
void setModuleStateLedState(bool state);  // Manual override (for solved state, etc.)
```

## Implementation Details

### Discovery Flow

1. **Module Initialization**
   ```cpp
   moduleState.begin(STATUS_LED_PIN);
   // Automatically starts sending MODULE_REGISTER every 1s
   ```

2. **Timer Discovery**
   - Timer receives `MODULE_REGISTER` or `MODULE_HEARTBEAT`
   - Timer sends `TIMER_MODULE_DISCOVERED` to module
   - Module stops LED flashing, marks as discovered

3. **State Broadcast**
   - Timer sends serial number, edgework, strike count
   - Module stores in internal state

### Status LED Behavior

| State | LED Behavior |
|-------|-------------|
| **Not Discovered** | Flash 500ms ON / 500ms OFF |
| **Discovered, Game Not Running** | LED OFF |
| **Game Running** | LED OFF (normal) |
| **Strike Event** | Flash RED once (500ms) |
| **Module Solved** | LED ON (solid) - optional, controlled by module |

### Heartbeat Timing

- **Discovery Mode**: 1000ms (1 second)
  - When: Not discovered OR game not running
  - Purpose: Fast discovery
  
- **Game Mode**: 5000ms (5 seconds)
  - When: Discovered AND game running
  - Purpose: Health monitoring

### CAN Message Handling

The library will automatically handle these timer messages:
- `TIMER_MODULE_DISCOVERED` - Discovery acknowledgment
- `TIMER_GAME_START` - Game started
- `TIMER_GAME_STOP` - Game stopped
- `TIMER_STRIKE_UPDATE` - Strike count changed
- `TIMER_SERIAL_NUMBER` - Serial number received
- `TIMER_TIME_UPDATE` - Time remaining (optional, may pass through)
- `TIMER_RESET` - Reset command (optional, may pass through)
- `TIMER_COUNTDOWN` - Initialization countdown (optional, may pass through)

The library will send:
- `MODULE_REGISTER` - Registration request
- `MODULE_HEARTBEAT` - Periodic heartbeat
- `MODULE_STATUS` - Status updates (optional, for enhanced reporting)
- `MODULE_SOLVED` - Module solved notification (via callback trigger)
- `MODULE_STRIKE` - Strike notification (via callback trigger)

### Edgework Storage

Edgework will be received via a new CAN message format (or parsed from existing messages). For now, we'll use the serial number message as a template.

**Proposed Edgework Message Format**:
```
TIMER_EDGEWORK = 0x18
[0] = TIMER_EDGEWORK
[1] = battery_count
[2] = indicator_count
[3..] = indicator_data (type, lit, label_length, label)
[..] = port_data (type, label_length, label)
```

**Alternative**: Send edgework via multiple messages or include in `TIMER_SERIAL_NUMBER` broadcast.

**Initial Approach**: Store edgework data in timer's `broadcastGameState()`, send as separate messages.

## Migration Path

### Step 1: Create New Library
- Create `shared_libs/module_state/` directory
- Implement `module_state.h` and `module_state.cpp`
- Keep `heartbeat` library for now (backward compatibility)

### Step 2: Update Simon Says Module
- Replace `heartbeat` includes with `module_state`
- Remove manual CAN message handling for timer messages
- Remove manual status LED control
- Use `moduleState.getSerialNumber()` and `moduleState.getEdgework()`
- Test thoroughly

### Step 3: Update Other Modules
- Migrate other modules to use `module_state`
- Remove duplicate code

### Step 4: Remove Heartbeat Library
- Delete `shared_libs/heartbeat/` directory
- Update documentation

## Usage Example

### Before (Simon Says - Current)
```cpp
#include <heartbeat.h>

bool gameRunning = false;
String serialNumber = "";
uint8_t currentStrikes = 0;
bool discovered = false;

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    if ((id == CAN_ID_TIMER || id == CAN_ID_BROADCAST) && len >= 1) {
        uint8_t msgType = data[0];
        switch (msgType) {
            case TIMER_GAME_START:
                gameRunning = true;
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

### After (Simon Says - New)
```cpp
#include <module_state.h>

ModuleState moduleState;

void onGameStateChange(bool running) {
    simonSays.onGameStateChange(running);
}

void onStrike(uint8_t strikes) {
    simonSays.setStrikeCount(strikes);
}

void onSerialNumber(const String& serial) {
    simonSays.setSerialNumber(serial);
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    // Let module_state handle timer messages
    moduleState.handleCanMessage(id, senderId, data, len);
    
    // Handle module-specific messages
    simonSays.handleCanMessage(id, senderId, data, len);
}

void setup() {
    moduleState.begin(SIMON_STATUS_LED);
    moduleState.setGameStateCallback(onGameStateChange);
    moduleState.setStrikeCallback(onStrike);
    moduleState.setSerialNumberCallback(onSerialNumber);
    
    // Serial number and edgework available immediately via:
    // moduleState.getSerialNumber()
    // moduleState.getEdgework()
}

void loop() {
    moduleState.update();
    simonSays.update();
    
    // Update module state
    moduleState.setStatus(MODULE_STATUS_ACTIVE);
    moduleState.setProgress(60);
    if (solved) {
        moduleState.setSolved(true);
    }
}
```

## Benefits

1. **Code Reduction**: Eliminate ~50-100 lines of boilerplate per module
2. **Consistency**: All modules have identical discovery/status LED behavior
3. **Maintainability**: Single place to update module-to-timer communication
4. **Easier Module Development**: New modules just need to implement game logic
5. **Better State Management**: Centralized serial number/edgework access
6. **Automatic Strike Handling**: Modules don't need to manually flash LED

## Open Questions

1. **Edgework Message Format**: How should edgework be sent from timer?
   - Option A: New `TIMER_EDGEWORK` message with structured data
   - Option B: Include in existing `broadcastGameState()` sequence
   - Option C: Send as JSON-like structured data

2. **Status LED Color**: How to handle different LED colors?
   - Option A: Only support single-color LED (current)
   - Option B: Support RGB LED with color parameter
   - **Decision**: Start with single-color, add RGB later if needed

3. **Strike Flash Color**: How to flash "red" on single-color LED?
   - **Decision**: For now, just flash ON/OFF. RGB support can be added later.

4. **Backward Compatibility**: Keep heartbeat library during transition?
   - **Decision**: Yes, keep both during migration, remove after.

5. **Module-Specific Messages**: How to handle module-specific CAN messages?
   - **Decision**: Library only handles timer messages. Module-specific messages pass through to module's handler.

## Implementation Checklist

- [ ] Create `shared_libs/module_state/` directory
- [ ] Implement `module_state.h` with full API
- [ ] Implement `module_state.cpp` with all functionality
- [ ] Add discovery LED flashing (500ms on/off)
- [ ] Add strike LED flashing (red once)
- [ ] Add heartbeat management (1s/5s timing)
- [ ] Add registration management
- [ ] Add serial number storage
- [ ] Add edgework storage (when format decided)
- [ ] Add callbacks for game state, strikes, serial number
- [ ] Create README.md with usage examples
- [ ] Test with Simon Says module
- [ ] Migrate Simon Says to use new library
- [ ] Test full discovery/heartbeat/status LED flow
- [ ] Update other modules (if any exist)
- [ ] Remove heartbeat library
- [ ] Update documentation

## Next Steps

1. **Review this plan** with team
2. **Decide on edgework message format**
3. **Create initial implementation**
4. **Test with Simon Says module**
5. **Iterate based on feedback**

