# KTANE hardware docs

Firmware notes for the physical Keep Talking and Nobody Explodes bomb. Each puzzle is its own Raspberry Pi Pico sketch. Modules share one CAN bus. The timer is the master.

The paper rulebook is still `DOCS/KeepTalkingAndNobodyExplodes-BombDefusalManual-v1.pdf`. This `docs/` tree is the hardware and firmware guide. Headers and `.cpp` files are the source of truth when an older README disagrees.

Bench sketches under `test_modules/` (`can_test_*`, `can_debug_*`) are not covered here.

## Start here

| If you want to… | Read |
|---|---|
| See how the bomb fits together | [Architecture](architecture/overview.md) |
| Wire the bus and flash a board | [Toolchain](getting-started/toolchain.md), [Bus wiring](getting-started/bus-wiring.md) |
| Understand a frame on the wire | [CAN protocol](architecture/can-protocol.md) |
| See how a module joins a game | [Module lifecycle](architecture/module-lifecycle.md) |

## Modules

| Module | Board | Role | Docs |
|---|---|---|---|
| `timer/` | Pico W | Master: countdown, strikes, edgework, WiFi setup | [Timer](modules/timer/README.md) |
| `simon_says/` | Pico | Gameplay puzzle. Template for the next puzzle | [Simon Says](modules/simon-says/README.md) |
| `serial_number/` | Pico | Fixed-id edgework tag (3-color e-paper) | [Serial number](modules/serial-number/README.md) |
| `audio/` | Pico W | Headphone audio from an SD sound bank | [Audio](modules/audio/README.md) |
| `audio_test/` | Pico W | Bench sketch for the same audio board. Not on the bomb protocol | [Audio test](modules/audio-test/README.md) |

## Shared libraries

| Library | Docs |
|---|---|
| `shared_libs/can_bus` | [CAN library](shared/can-bus.md) |
| `shared_libs/module_state` | [Module state](shared/module-state.md) |
| `shared_libs/ktane_console` | [Console](shared/console.md) |

`shared_libs/pins/` is an empty placeholder. Pin constants live in the module that owns the wiring.

## Planned modules

Type codes already exist in `shared_libs/can_bus/can_bus.h`. There is no firmware yet for wires, button, keypad, who's on first, memory, Morse, complicated wires, wire sequences, maze, password, needy venting gas / capacitor / knob, or the indicator, battery, and port edgework panels. Clone `simon_says/` for a new puzzle. See [Adding a module](architecture/adding-a-module.md).
