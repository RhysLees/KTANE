# Adding a module

Clone `simon_says/`. That sketch is the pattern: dynamic CAN id, `module_state`, puzzle rules from the paper manual, sounds sent to the audio module.

## Project

1. New top-level directory with its own `platformio.ini`.
2. Set `build_dir = ../build/my_module` and `default_envs = my_module`.
3. `board = rpipico` unless the sketch needs WiFi or Bluetooth, in which case `board = rpipicow`.
4. Keep `lib_extra_dirs = ../shared_libs`, `coryjfowler/mcp_can@^1.5.1`, `upload_protocol = cmsis-dap`, and `-DKTANE_PICOPROBE_UART`.
5. `build_all.ps1` picks up any new root directory that contains `platformio.ini`.

```ini
[platformio]
build_dir = ../build/my_module
default_envs = my_module

[env]
framework = arduino
monitor_speed = 115200
upload_protocol = cmsis-dap
monitor_flags = --echo
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
board = rpipico
board_build.core = earlephilhower
build_flags =
    -DKTANE_PICOPROBE_UART
lib_deps =
    coryjfowler/mcp_can@^1.5.1
lib_extra_dirs =
    ../shared_libs

[env:my_module]
debug_tool = cmsis-dap
build_type = debug
debug_build_flags = -O0 -ggdb3 -g3
```

Build from the module directory with `pio run`.

## Type code

If `can_bus.h` does not already reserve a type, add one in an open range and append it to `CAN_MODULE_TYPES`. Gameplay types still free inside `0x10`–`0x1A` are none: wires through password are all assigned. The next classic puzzle needs a reserved slot (`0x1B`–`0x1F`) promoted on purpose.

`timer/lib/game_state/game_state.h` has a parallel `ModuleType` enum. Its edgework values (`0x40` and up) are not wire ids. When you teach the timer about a new puzzle, keep the numeric value equal to `CAN_TYPE_*`.

## Sketch

Match `simon_says/src/main.cpp`.

```cpp
#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>
#include <module_state.h>

constexpr int STATUS_LED_PIN = 11; // MODULE_STATE_NO_LED if this board has no status LED

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  moduleStateHandleCanMessage(id, senderId, data, len);
}

void setup() {
  ktaneConsoleInit(115200);
  initCanBus(CAN_INSTANCE_ID(CAN_TYPE_WIRES, 0x00));
  assignUniqueId(CAN_TYPE_WIRES);
  registerCanCallback(onCanMessage);
  initModuleState(STATUS_LED_PIN);
  if (globalModuleState) {
    globalModuleState->setGameStateCallback(onGameStateChanged);
    globalModuleState->setStrikeCallback(onStrikeCountUpdated);
    globalModuleState->setSerialNumberCallback(onSerialNumberUpdated);
  }
}

void loop() {
  handleCanMessages();
  updateModuleState();
  // read inputs, then setModuleStateStatus / setModuleStateProgress / setModuleStateSolved
  // wrong input: globalModuleState->triggerStrike()
}
```

Fixed-id modules (edgework panels) pass `CAN_ID_*` to `initCanBus` and skip `assignUniqueId`. They still call `initModuleState`.

Keep the puzzle library free of `module_state.h` if that include would cycle. Simon's library includes `can_bus.h` only. `main.cpp` includes `module_state.h` and translates phases into status.

## Pins

CAN already owns SPI0: CS GP17, INT GP20, MISO GP16, SCK GP18, MOSI GP19. Console owns GP0 and GP1 when `-DKTANE_PICOPROBE_UART` is set. Put a second SPI device on SPI1. Set `Wire` pins before `Wire.begin()`.

## Rules

Read the puzzle from `DOCS/KeepTalkingAndNobodyExplodes-BombDefusalManual-v1.pdf`. Seed the puzzle from `getSerialNumber()`, `getStrikeCount()`, and `getEdgework()` inside the callbacks, not at `setup`, because those values arrive after discovery.
