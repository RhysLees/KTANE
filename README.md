# KTANE (Keep Talking and Nobody Explodes) - Hardware Implementation

A complete hardware implementation of the Keep Talking and Nobody Explodes bomb defusal game using multiple microcontrollers connected via CAN bus.

## 🎮 Project Overview

This project recreates the cooperative puzzle game KTANE in physical hardware form. Players must communicate to defuse a "bomb" consisting of multiple puzzle modules, each requiring different solving strategies based on a shared manual.

### Key Features
- **Modular Design**: Individual microcontroller-based modules connected via CAN bus
- **Scalable Architecture**: Easy to add new module types  
- **Real Hardware**: Physical buttons, LEDs, displays, and audio feedback
- **Authentic Experience**: Faithful recreation of the original game mechanics
- **Cooperative Gameplay**: Requires team communication and manual consultation

## 🏗️ Architecture

### System Components
```
┌─────────────┐    ┌─────────────┐    ┌─────────────┐
│    Timer    │    │   Modules   │    │   Audio     │
│   Module    │◄──►│ (Simon Says,│◄──►│   System    │
│  (Master)   │    │  Wires, etc)│    │             │
└─────────────┘    └─────────────┘    └─────────────┘
        ▲                   ▲                   ▲
        │                   │                   │
        └───────────── CAN Bus ─────────────────┘
                            │
               ┌─────────────▼─────────────┐
               │    Serial Display         │
               │   (Bomb Information)      │
               └───────────────────────────┘
```

### Current Modules
- **Timer Module**: Master controller managing game state, countdown, and strikes
- **Simon Says**: Memory-based sequence matching puzzle  
- **Audio System**: Centralized sound effects and music playback
- **Serial Display**: E-paper display showing bomb serial number and edgework
- **More modules**: Additional puzzle types can be easily added

## 📚 Shared Libraries

The project uses a sophisticated shared library system for code reuse and consistency:

### 🚌 **[CAN Bus Library](shared_libs/can_bus/)**
- Core communication system between all modules
- Automatic ID negotiation for dynamic modules
- Connection detection and timeout handling
- Standardized message formats

### ⚙️ **[Module State Library](shared_libs/module_state/)**  
- Unified heartbeat, registration, and status management
- Automatic status, progress, and solved reporting
- Optional status LED handling per module
- Built on CAN bus for reliable communication

**[📖 Complete Shared Libraries Documentation](shared_libs/)**

## 🚀 Quick Start

### Building All Modules
```bash
# Build all firmware files at once
./build_all.ps1

# Individual module build (from module directory)
cd timer
pio run
```

### Uploading Firmware
```bash
# Upload to connected board (from module directory)  
cd simon_says
pio run --target upload
```

### Hardware Setup
1. **Connect CAN Bus**: Wire all modules to shared CAN bus with proper termination
2. **Power Modules**: Ensure all modules have adequate power supply
3. **Upload Firmware**: Flash each module with its respective firmware
4. **Test Communication**: Use timer module serial console to verify connections

## 🔧 Development

### Adding New Modules

1. **Create Module Directory**:
```bash
mkdir my_new_module
cd my_new_module
pio project init --board your_board
```

2. **Add Shared Libraries** to `platformio.ini`:
```ini
[env:your_board]
lib_deps = 
    ../shared_libs/can_bus
    ../shared_libs/module_state
```

3. **Follow Integration Pattern**:
```cpp
#include <Arduino.h>
#include <can_bus.h>
#include <module_state.h>

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    moduleStateHandleCanMessage(id, senderId, data, len);
    // Handle CAN messages specific to your module
}

void setup() {
    Serial.begin(115200);
    
    // Initialize CAN bus
    initCanBus(CAN_INSTANCE_ID(CAN_TYPE_YOUR_MODULE, 0x00));
    registerCanCallback(onCanMessage);
    assignUniqueId(CAN_TYPE_YOUR_MODULE);
    
    // Initialize module state manager (handles registration + heartbeat)
    initModuleState(STATUS_LED_PIN);
}

void loop() {
    handleCanMessages();
    updateModuleState();
    // Your module logic here
}
```

### Module Types and IDs
See [CAN Bus Library Documentation](shared_libs/can_bus/) for complete list of:
- Module type definitions
- Message type constants  
- CAN ID structure
- Communication protocols

## 🛠️ Hardware Requirements

### Per Module
- **Microcontroller**: RP2040, ESP32, or Arduino-compatible
- **CAN Controller**: MCP2515 SPI-based CAN controller
- **CAN Transceiver**: MCP2551 or compatible
- **Module-Specific Components**: LEDs, buttons, displays per module requirements

### System-Wide  
- **CAN Bus Wiring**: Twisted pair with 120Ω termination at both ends
- **Power Distribution**: 5V/3.3V power supply for all modules
- **Common Ground**: Shared ground reference for all modules

## 📁 Project Structure

```
KTANE/
├── shared_libs/              # Shared libraries for all modules
│   ├── can_bus/             # Core CAN communication
│   ├── heartbeat/           # Module health monitoring  
│   └── README.md            # Library documentation
├── timer/                   # Master timer/game controller
├── simon_says/             # Simon Says puzzle module
├── audio/                  # Centralized audio system
├── serial_number/          # Bomb information display
├── firmware/               # Compiled firmware files (.uf2)
├── DOCS/                   # Game manuals and documentation
├── build_all.ps1          # Build automation script
└── BUILD_README.md        # Build system documentation
```

## 🎯 Game Mechanics

### Core Gameplay
1. **Setup**: Modules randomly generate puzzles based on bomb serial number
2. **Defusal**: Players solve modules using information from the manual
3. **Communication**: One player sees the bomb, others have the manual
4. **Time Pressure**: Complete all modules before timer reaches zero
5. **Strikes**: Mistakes add strikes; 3 strikes = explosion

### Module Categories
- **Regular Modules**: Must be solved to defuse the bomb
- **Needy Modules**: Require periodic attention to prevent strikes
- **Edgework**: Serial number, indicators, ports used by various modules

## 🔍 Debugging and Monitoring

### Timer Module Console
The timer module provides a comprehensive debug interface:
```
CAN       - Show CAN bus status and module connections
STATUS    - Display game state and module information  
START     - Begin game countdown
RESET     - Reset all modules and game state
HELP      - Show all available commands
```

### Connection Monitoring
- Real-time module connection status
- Heartbeat monitoring with timestamps
- Automatic timeout detection and recovery
- Debug output for troubleshooting communication issues

## 📈 Performance

### System Specifications
- **CAN Bus Speed**: 500 kbps
- **Typical Message Load**: 10-15 messages/second
- **Module Response Time**: <10ms for critical operations
- **Connection Timeout**: 5 seconds
- **Memory Overhead**: <1% per shared library

## 🤝 Contributing

1. **Follow Code Style**: Use consistent formatting and naming conventions
2. **Use Shared Libraries**: Leverage existing CAN bus and heartbeat systems
3. **Document APIs**: Include comprehensive documentation for new libraries
4. **Test Integration**: Verify compatibility with existing modules
5. **Update Documentation**: Keep README files current with changes

## 📄 License

This project is open source. See individual module directories for specific licensing information.

## 🔗 Resources

- [Official KTANE Game](https://keeptalkinggame.com/)
- [Bomb Defusal Manual](DOCS/KeepTalkingAndNobodyExplodes-BombDefusalManual-v1.pdf)
- [Shared Libraries Documentation](shared_libs/)
- [Build System Guide](BUILD_README.md)
- [PlatformIO Documentation](https://docs.platformio.org/)

---

*Keep Talking and Nobody Explodes - Now in hardware form! 💣* 