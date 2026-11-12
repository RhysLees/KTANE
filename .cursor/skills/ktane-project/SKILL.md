---
name: ktane-project
description: >-
  Architecture, module map, and firmware rules for this Keep Talking and
  Nobody Explodes hardware repo. Use when working anywhere in KTANE, adding a
  bomb module, changing CAN, PlatformIO, pins, or shared libraries, or when
  the user mentions KTANE, the bomb, a module, the timer, audio, serial
  display, or Simon Says.
---

# KTANE hardware

Physical Keep Talking and Nobody Explodes. Each puzzle is its own Raspberry Pi Pico (or Pico W) sketch. Modules share one CAN bus. The timer is the master. Players still use the paper manual in `DOCS/KeepTalkingAndNobodyExplodes-BombDefusalManual-v1.pdf`.

Every sketch is Arduino on the Earle Philhower core (`board_build.core = earlephilhower`), so write `setup` / `loop`, `Wire`, `SPI`, and `Serial`. Read [arduino-pico](../arduino-pico/SKILL.md) before adding peripherals. Leave Pico SDK `CMakeLists.txt`, `pico_stdlib`, and the ArduinoCore-mbed core unused.

Headers and `.cpp` files are the source of truth. Several READMEs still describe older message IDs, heartbeat times, and Simon pinouts.

## Layout

| Path | Role |
|---|---|
| `modules/timer/` | Pico W master. Countdown, strikes, edgework, WiFi setup UI. Does not use `module_state`. |
| `modules/simon_says/` | Gameplay module. Pattern for every future puzzle module. |
| `edgework/serial_number/` | Fixed-ID edgework display (3-color e-paper). |
| `support/audio/` | Fixed-ID sound module (TLV320 + SD). Real-life addition, not a manual puzzle. |
| `support/audio_test/` | Bench sketch: one WAV from SD through the same codec. |
| `support/debugger/can_test_*`, `can_debug_*` | MCP2515 and `can_bus` harness sketches (`build_debug.ps1`). |
| `shared_libs/can_bus/` | Wire protocol and MCP2515 driver. |
| `shared_libs/module_state/` | Registration, heartbeat, strikes, serial, status LED. Every module except the timer. |
| `shared_libs/ktane_console/` | Log stream (`Serial` or Picoprobe `Serial1`). |
| `shared_libs/pins/` | Empty placeholder. Pin constants live in the module that owns the wiring. |
| `firmware/` | UF2 files copied by `build_all.ps1`. |
| `build/<module>/` | PlatformIO build output (`build_dir` in each `platformio.ini`). |

Planned manual modules already have CAN type codes and no firmware yet: wires, button, keypad, who's on first, memory, Morse, complicated wires, wire sequences, maze, password, needy venting gas / capacitor / knob, and edgework panels (indicators, batteries, ports). Add each under `modules/`, cloned from `modules/simon_says/`.

## Bus

11-bit id: `(moduleType << 5) | instance`. Type is 6 bits, instance is 5 bits.

- Infrastructure (timer `0x00`, audio `0x01`) and edgework (`0x20`–`0x2F`) use a fixed instance 0: `CAN_ID_TIMER`, `CAN_ID_AUDIO`, `CAN_ID_SERIAL_DISPLAY`.
- Gameplay and needy modules call `assignUniqueId(CAN_TYPE_...)` so two copies can share the bus. Instance 0 is only the negotiation address.
- `CAN_ID_BROADCAST` (`0x7E0`) is the timer's fan-out address.

`sendCanMessage` queues the caller's bytes and returns without waiting on the controller or the wire. `handleCanMessages` loads free TX buffers, drains RX, and advances id negotiation. Do not use `delay` on the core that owns CAN. The caller's payload is at most 6 bytes; the sender's 16-bit id is prefixed when the buffer is loaded. Callbacks already have that prefix removed: `data[0]` is the message type. Speed is 500 kbit/s, MCP2515 crystal 8 MHz, SPI0 CS GP17, INT GP20. Only the two modules at the physical ends of the bus enable the J1 120 ohm jumper.

Full codes and payloads: [ktane-can-bus](../ktane-can-bus/SKILL.md). Module lifecycle: [ktane-module-state](../ktane-module-state/SKILL.md).

`timer/lib/game_state/game_state.h` lists side modules as `0x40`–`0x43`. Those values are not on the wire. Encode and decode with the constants in `shared_libs/can_bus/can_bus.h`.

## Gameplay module skeleton

Match `modules/simon_says/src/main.cpp`.

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
  initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
  assignUniqueId(CAN_TYPE_SIMON);
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

Fixed-id modules (audio, serial display, future edgework panels) pass `CAN_ID_*` to `initCanBus` and skip `assignUniqueId`. They still call `initModuleState`.

`platformio.ini` for a new module:

```ini
[platformio]
build_dir = ../../build/my_module
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
    ../../shared_libs

[env:my_module]
debug_tool = cmsis-dap
build_type = debug
debug_build_flags = -O0 -ggdb3 -g3
```

Use `board = rpipicow` only when the sketch needs WiFi or Bluetooth. Build from the module directory with `pio run`. `build_all.ps1` scans `modules/`, `edgework/`, and `support/` (except `support/debugger/`) and copies UF2 files to `firmware/`.

## Rules that keep the bomb in sync

- Call `ktaneConsoleInit` once, then log with `KTANE_CONSOLE_OUT`. With `-DKTANE_PICOPROBE_UART`, that is `Serial1` on GP0/GP1. Leave GP0 and GP1 free on every board.
- Call `handleCanMessages()` and, except on the timer, `updateModuleState()` on every pass of the core that owns CAN. A core blocked in a multi-second e-paper refresh or an SD read must not be the only place those run.
- Publish puzzle state through `setModuleStateStatus`, `setModuleStateProgress`, and `setModuleStateSolved`. Play sounds by sending a one-byte `AudioMessage` to `CAN_ID_AUDIO`, or `globalModuleState->playAudio(...)`.
- Generate puzzles from `getSerialNumber()`, `getStrikeCount()`, and `getEdgework()` after the timer has sent them. Read the manual in `DOCS/` for the real rule table.
- `randomSeed(rp2040.hwrand32())` on boards that need entropy. The RP2040 boot state is a poor seed.
- Set `Wire` / `SPI` pins before `begin()`. `can_bus` calls `SPI.begin()` itself and owns SPI0. Put SD cards and e-paper on SPI1.
- Two cores share the die. `setup1` / `loop1` is core 1. Flash writes (`EEPROM.commit`) stall core 1. Lock SdFat with the mutex in `support/audio/lib/sd_card` if both cores touch the card.
- Keep ArduinoJson on major version 6 (`bblanchon/ArduinoJson@^6.21.0`). Version 7 removes `DynamicJsonDocument`.

## Which skill to open

| You are about to... | Skill |
|---|---|
| Add pins, WiFi, a second core, EEPROM, or any Arduino API | [arduino-pico](../arduino-pico/SKILL.md) |
| Send or parse a bomb message | [ktane-can-bus](../ktane-can-bus/SKILL.md) |
| Register, heartbeat, strikes, serial, edgework | [ktane-module-state](../ktane-module-state/SKILL.md) |
| Talk to the MCP2515 directly (harness only) | [mcp-can](../mcp-can/SKILL.md) |
| Draw text or shapes | [adafruit-gfx](../adafruit-gfx/SKILL.md) |
| Change the countdown digits | [adafruit-led-backpack](../adafruit-led-backpack/SKILL.md) |
| Change the timer web API | [arduinojson](../arduinojson/SKILL.md) |
| Change the serial tag | [gxepd2](../gxepd2/SKILL.md) |
| Change I2S, WAV decode, or the mixer | [arduino-audio-tools](../arduino-audio-tools/SKILL.md) |
| Change the headphone codec | [adafruit-tlv320](../adafruit-tlv320/SKILL.md) |
| Read or write the SD card | [sdfat](../sdfat/SKILL.md) |
| Edit one module folder | `module-timer`, `module-simon-says`, `module-serial-number`, `module-audio`, `module-audio-test`, `module-can-harness` |
