# KTANE Simon Says Module

A complete implementation of the Simon Says module for the Keep Talking and Nobody Explodes (KTANE) project.

## Overview

This Simon Says module implements the classic memory game where players must repeat sequences of colored button presses. The module follows the official KTANE rules with proper strike handling, serial number integration, and CAN bus communication with realtime status updates.

## Features

### 🎮 **Complete Game Logic**
- **Sequence Generation**: Starts with 3 colors, adds 1 per stage up to 5 total
- **Color Display**: Visual feedback with configurable timing
- **Input Validation**: Real-time button press detection and validation
- **Strike Handling**: Immediate strike reporting to module state
- **KTANE Rules**: Full implementation of official color mapping rules

### 🔧 **Hardware Interface**
- **4 Colored LEDs**: Red, Yellow, Green, Blue visual feedback
- **4 Push Buttons**: Corresponding input buttons with debouncing
- **Status LED**: Module state indication
- **Audio via CAN**: All audio handled by dedicated audio module

### 🎯 **KTANE Rule Implementation**

The module implements the official Simon Says rules based on strikes and serial number:

---

### **If the serial number *contains a vowel*:**

| Flashing Color | 0 Strikes → Press | 1 Strike → Press | 2+ Strikes → Press |
|----------------|------------------|------------------|--------------------|
| Red            | Blue             | Yellow           | Green              |
| Blue           | Red              | Green            | Red                |
| Green          | Yellow           | Blue             | Yellow             |
| Yellow         | Green            | Red              | Blue               |

---

### **If the serial number does *not* contain a vowel:**

| Flashing Color | 0 Strikes → Press | 1 Strike → Press | 2+ Strikes → Press |
|----------------|------------------|------------------|--------------------|
| Red            | Blue             | Red              | Yellow             |
| Blue           | Yellow           | Blue             | Green              |
| Green          | Green            | Yellow           | Blue               |
| Yellow         | Red              | Green            | Red                |

---

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

## Audio System

Audio is handled entirely by the dedicated audio module via CAN bus messages:

- **Button Press Sounds**: Different audio types for each color
  - Red: `AUDIO_PLAY_BEEP_NORMAL`
  - Yellow: `AUDIO_PLAY_BEEP_FAST`
  - Green: `AUDIO_PLAY_BEEP_HIGH`
  - Blue: `AUDIO_PLAY_CORRECT_TIME`
- **Strike Sound**: `AUDIO_PLAY_STRIKE`
- **Solved Sound**: `AUDIO_PLAY_DEFUSED`

## Game Flow

1. **Initialization**: Module registers with game state manager
2. **Game Start**: Receives start signal from timer module
3. **Sequence Generation**: Creates random 3-color sequence
4. **Display & Input Phase**: Shows sequence with LEDs and audio via CAN and check for player button presses at the same time (an input will interupt the display sequence)
6. **Validation**: Checks input against expected sequence (with rule transformations)
7. **Strike Handling**: Immediate strike notification to timer module on wrong input
8. **Progression**: Adds one color and repeats until 5 colors total
9. **Victory**: Module marked as solved when all 5 stages completed

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
- Shared CAN Bus Library
- Shared Module State Library

## Realtime Operation

The module operates with realtime status updates instead of periodic heartbeats:

- **State Changes**: Immediate status update when state changes
- **Solve Status**: Immediate notification when solved status changes
- **Sequence Progress**: Immediate update when sequence length changes
- **Strike Events**: Immediate strike notification to timer module

This ensures the game state manager always has current information without polling delays.

### Strike Handling
```cpp
// When wrong input detected, module automatically:
// 1. Increments internal strike count
// 2. Sends strike message to timer module
// 3. Plays strike audio via audio module
// 4. Updates status in realtime
// 5. Flashes all LEDs

// Note: not completing all the inputs does not count as a strike only if the sequence is input wrong does that count
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

## License

This project is part of the KTANE hardware implementation and follows the same licensing as the main project. 