---
name: module-serial-number
description: >-
  Serial-number edgework module (edgework/serial_number/): fixed CAN id, 3-color
  e-paper tag on SPI1, drawing on core 1. Use when editing edgework/serial_number/,
  the tag layout, or SERIAL_DISPLAY commands.
---

# Serial number module

`edgework/serial_number/` on a Pico (`board = rpipico`). Fixed id `CAN_ID_SERIAL_DISPLAY` (`CAN_TYPE_SERIAL_DISPLAY` `0x20`, instance 0). It is edgework, not a solvable puzzle. `initModuleState(MODULE_STATE_NO_LED)`.

Build flag `-DEDGE_SERIAL_NUMBER` is set. Current sources do not branch on it.

## What main.cpp does

Core 0 (`setup` / `loop`): console, `Wire` on GP8/GP9, `initCanBus(CAN_ID_SERIAL_DISPLAY)`, CAN callback, module state, serial-number callback. `loop` is only `handleCanMessages()` and `updateModuleState()`.

Core 1 (`setup1` / `loop1`): `epaperTaskSetup` / `epaperTaskLoop`, which drain a 4-deep command queue and refresh the panel. A refresh takes seconds. Leaving it on core 0 would stall discovery.

The serial string gets on the glass two ways:

- `module_state` reassembles `TIMER_SERIAL_NUMBER_FIRST_HALF` and `TIMER_SERIAL_NUMBER_LAST_HALF` and calls `onSerialNumberReceived`, which queues `epaperDrawTag`.
- A direct `SERIAL_DISPLAY_SET_SERIAL` (`0x30`) with 6 bytes still queues a tag. That payload is 7 bytes and does not fit in `sendCanMessage`, so the timer path is the halves. `SERIAL_DISPLAY_CLEAR` and `SERIAL_DISPLAY_SHOW_CREDIT` fit in one byte.

While a display command is handled, status flips to `MODULE_STATUS_ACTIVE` and back to `IDLE` in the same callback. The long work is the core-1 refresh, not this callback.

## Panel

`GxEPD2_3C<GxEPD2_266c, GxEPD2_266c::HEIGHT>` on SPI1. Pins: CS GP5, DC GP6, RST GP7, BUSY GP8, SCK GP10, MOSI GP11. `init(..., SPI1, SPISettings(115200, MSBFIRST, SPI_MODE0))`, rotation 1, full window, `hibernate()` after each frame.

Tag layout: top half filled `GxEPD_RED` with "SERIAL #" in `resolution_medium24pt7b` white, bottom half the 6-character serial in `resolution_medium36pt7b` black. Credit frame says "KTANE IRL" / "By Rhys Lees". Fonts are `GFXfont` headers in `lib/fonts/`.

Draw API details: [gxepd2](../gxepd2/SKILL.md) and [adafruit-gfx](../adafruit-gfx/SKILL.md).

## Changing the picture

Add a command enum value in `can_bus.h` (`CanSerialDisplayCommand`) and a queue command in `epaper.cpp`. Enqueue from core 0 only. Draw inside `execute*` on core 1, inside `firstPage` / `nextPage`, then `hibernate()`. Keep the queue atomic head/tail. A full queue returns false from `epaperDrawTag`; the caller currently ignores that.

`Wire.begin()` in `setup` is not used by the panel. Leave it if a later sensor shares I2C0. Do not point the panel at `SPI`.
