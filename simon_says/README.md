# KTANE Simon Says Module

A complete implementation of the Simon Says module for the Keep Talking and Nobody Explodes (KTANE) project.

## Overview

This Simon Says module implements the classic memory game where players must repeat sequences of colored button presses. The module follows the official KTANE rules with proper strike handling, serial number integration, and CAN bus communication with realtime status updates.

## Features

### 🎮 **Complete Game Logic**
- **Sequence Generation**: Starts with 3 colors, adds 1 per stage up to 5 total
- **Color Display**: Visual feedback with configurable timing
- **Input Validation**: Real-time button press detection and validation
- **Strike Handling**: Immediate strike reporting to timer module
- **KTANE Rules**: Full implementation of official color mapping rules

### 🔧 **Hardware Interface**
- **4 Colored LEDs**: Red, Yellow, Green, Blue visual feedback
- **4 Push Buttons**: Corresponding input buttons with debouncing
- **Status LED**: Module state indication
- **Audio via CAN**: All audio handled by dedicated audio module

### 📡 **CAN Bus Integration**
- **Registration**: Automatic registration with game state manager
- **Realtime Status**: Immediate status updates on state changes
- **Strike Reporting**: Immediate strike notification to timer module
- **Audio Messages**: Color sounds and strike/solve audio via audio module
- **Game State**: Integration with master timer and game flow

### 🎯 **KTANE Rule Implementation**

The module implements the official Simon Says rules based on strikes and serial number:

#### No Strikes
- **Serial has vowel**: Red↔Blue swap, Yellow/Green unchanged
- **No vowel**: Red→Blue→Green→Yellow→Red cycle

#### 1 Strike
- **Serial has vowel**: Red→Yellow, Blue→Green, others unchanged
- **No vowel**: Red→Blue→Green→Yellow→Red cycle

#### 2+ Strikes
- **Serial has vowel**: Red→Yellow, Green→Blue, others unchanged
- **No vowel**: Red→Blue→Green→Yellow→Red cycle

## Hardware Connections

### LEDs
- **Red LED**: Pin 2
- **Yellow LED**: Pin 3
- **Green LED**: Pin 4
- **Blue LED**: Pin 5
- **Status LED**: Pin 11

### Buttons
- **Red Button**: Pin 6 (INPUT_PULLUP)
- **Yellow Button**: Pin 7 (INPUT_PULLUP)
- **Green Button**: Pin 8 (INPUT_PULLUP)
- **Blue Button**: Pin 9 (INPUT_PULLUP)

### CAN Bus
- **SPI CS**: Pin 17
- **Interrupt**: Pin 20

## Audio System

Audio is handled entirely by the dedicated audio module via CAN bus messages:

- **Button Press Sounds**: Different audio types for each color
  - Red: `AUDIO_BEEP_NORMAL`
  - Yellow: `AUDIO_BEEP_FAST`
  - Green: `AUDIO_BEEP_HIGH`
  - Blue: `AUDIO_CORRECT_TIME`
- **Strike Sound**: `AUDIO_STRIKE`
- **Solved Sound**: `AUDIO_DEFUSED`

## Game Flow

1. **Initialization**: Module registers with game state manager
2. **Game Start**: Receives start signal from timer module
3. **Sequence Generation**: Creates random 3-color sequence
4. **Display Phase**: Shows sequence with LEDs and audio via CAN
5. **Input Phase**: Waits for player button presses
6. **Validation**: Checks input against expected sequence (with rule transformations)
7. **Strike Handling**: Immediate strike notification to timer module on wrong input
8. **Progression**: Adds one color and repeats until 5 colors total
9. **Victory**: Module marked as solved when all 5 stages completed

## CAN Bus Messages

### Outgoing Messages
- `0x01` - Registration (module type and instance)
- `0x02` - Module solved notification
- `0x03` - Strike occurred (with count)
- `0x04` - Realtime status update
- `0x06` - Status update
- Audio messages to `CAN_ID_AUDIO` with sound types

### Incoming Messages
- `0x10` - Game start signal
- `0x11` - Game stop signal
- `0x12` - Strike count update
- `0x13` - Serial number update
- `0x14` - Reset command

### Strike Communication
When a wrong input is detected, the module immediately sends:
1. Strike message to itself (`SIMON_MSG_STRIKE`)
2. Strike update directly to timer module (`0x12` with strike count)
3. Realtime status update with new state

## Serial Commands

The module supports these debug commands via serial console:

```
STATUS       - Show module status
SEQUENCE     - Show current sequence
RULES        - Show color mapping rules
RESET        - Reset module
SERIAL <xxx> - Set serial number
STRIKES <n>  - Set strike count (0-3)
START        - Start game
STOP         - Stop game
HELP         - Show command help
```

## Configuration

### Timing Settings
```cpp
#define SIMON_DISPLAY_TIME_MS     800   // LED on time
#define SIMON_PAUSE_TIME_MS       200   // Pause between colors
#define SIMON_INPUT_TIMEOUT_MS    5000  // Input timeout
#define SIMON_STRIKE_FLASH_MS     1000  // Strike flash duration
```

### Game Settings
```cpp
#define SIMON_MAX_SEQUENCE_LENGTH 5     // Maximum sequence length
```

## Building and Installation

### Prerequisites
- PlatformIO
- Raspberry Pi Pico
- CAN bus transceiver (MCP2515)
- Hardware components (LEDs, buttons)

### Build Process
```bash
cd simon_says
pio build
pio upload
```

### Dependencies
- Arduino framework
- SPI library
- Wire library
- Seeed CAN library
- Shared CAN bus library

## Realtime Operation

The module operates with realtime status updates instead of periodic heartbeats:

- **State Changes**: Immediate status update when state changes
- **Solve Status**: Immediate notification when solved status changes
- **Sequence Progress**: Immediate update when sequence length changes
- **Strike Events**: Immediate strike notification to timer module

This ensures the game state manager always has current information without polling delays.

## Usage Example

### Basic Integration
```cpp
#include <simon_says.h>

SimonSays module;

void setup() {
    module.begin();
    module.setSerialNumber("A1B2C3");
    module.setStrikeCount(0);
}

void loop() {
    module.update(); // Handles realtime status updates
    // Handle CAN messages, serial commands, etc.
}
```

### Strike Handling
```cpp
// When wrong input detected, module automatically:
// 1. Increments internal strike count
// 2. Sends strike message to timer module
// 3. Plays strike audio via audio module
// 4. Updates status in realtime
// 5. Flashes all LEDs
```

## State Machine

The module operates with these internal states:

- **IDLE**: Waiting for game to start
- **GENERATING**: Creating new sequence
- **DISPLAYING**: Showing sequence to player
- **WAITING_INPUT**: Waiting for button presses
- **CHECKING_INPUT**: Validating input
- **CORRECT_SEQUENCE**: Sequence completed correctly
- **WRONG_INPUT**: Incorrect input detected
- **SOLVED**: Module completely solved
- **STRIKE**: Error state with visual feedback

## Debugging

### Serial Output
The module provides detailed logging:
```
Simon Says: Initializing...
Simon Says: Registration sent
Simon Says: Ready!
Simon Says: Game starting...
Simon Says: Generating sequence...
Simon Says: Sequence (3): RED -> BLUE -> GREEN
Simon Says: Strike!
Simon Says: Strike message sent
```

### Status Commands
Use `STATUS` command to see current state:
```
=== SIMON SAYS STATUS ===
State: WAITING_INPUT
Solved: NO
Game Started: YES
Sequence Length: 3
Strikes: 0
Has Vowel in Serial: YES
Current Strikes: 0
```

### Rule Debugging
Use `RULES` command to see current color mappings:
```
=== SIMON SAYS RULES ===
Strikes: 0
Serial has vowel: YES
Color mappings:
  RED -> BLUE
  YELLOW -> YELLOW
  GREEN -> GREEN
  BLUE -> RED
```

## File Structure

```
simon_says/
├── lib/
│   └── simon_says/
│       ├── simon_says.h      # Header file
│       └── simon_says.cpp    # Implementation
├── src/
│   └── main.cpp              # Main program
├── platformio.ini            # PlatformIO configuration
└── README.md                 # This file
```

## Contributing

When modifying the module:

1. Follow the existing code style and patterns
2. Update timing constants for hardware changes
3. Test all KTANE rule combinations
4. Verify CAN bus message compatibility
5. Update documentation for any API changes
6. Ensure realtime status updates work correctly

## Troubleshooting

### Common Issues

1. **No CAN communication**: Check wiring and bus termination
2. **Incorrect sequences**: Verify random seed initialization
3. **Button not responding**: Check pull-up resistors and debouncing
4. **LED not lighting**: Verify pin assignments and current limiting
5. **No audio**: Check CAN communication with audio module
6. **Strikes not registering**: Verify CAN messages to timer module

### Debug Steps

1. Use serial monitor to check initialization
2. Send `STATUS` command to verify state
3. Use `RULES` command to check color mappings
4. Monitor CAN bus traffic for audio and strike messages
5. Check hardware connections with multimeter
6. Verify realtime status updates are being sent

## License

This project is part of the KTANE hardware implementation and follows the same licensing as the main project. 