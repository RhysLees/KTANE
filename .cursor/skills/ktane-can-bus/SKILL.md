---
name: ktane-can-bus
description: >-
  KTANE CAN protocol in shared_libs/can_bus: 11-bit ids, 6-byte payloads,
  timer and module message types, id negotiation. Use when sending or
  receiving bomb messages, adding a module type, or editing can_bus.h.
  Wire ids come from can_bus.h. Gameplay code does not call MCP_CAN.
---

# can_bus

`shared_libs/can_bus/`. Include `<can_bus.h>`. Link it by `lib_extra_dirs = ../shared_libs`. The folder is a PlatformIO library (`library.properties` is not required because `lib_extra_dirs` compiles the sources).

`initCanBus` configures the MCP2515 (see [mcp-can](../mcp-can/SKILL.md)) at 500 kbit/s, 8 MHz, CS GP17, INT GP20, one-shot TX, and attaches a falling-edge ISR. It does not `delay` or recurse if `begin` fails; `handleCanMessages` retries once a second.

Masks accept this module's type (any instance) and `CAN_ID_BROADCAST`. A registered raw callback opens the masks again so that node can see other destinations. SPI reads of RX buffers are one transaction (`READ RX BUFFER`), and TX is a load plus RTS with no wait for TXREQ to clear.

## Id layout

```text
10        5 4        0
MMMMMM IIIII
type     instance
```

```cpp
constexpr uint16_t CAN_INSTANCE_ID(uint8_t moduleType, uint8_t instanceId);
// ((moduleType & 0x3F) << 5) | (instanceId & 0x1F)
```

| Constant | Value | Who |
|---|---|---|
| `CAN_TYPE_TIMER` | `0x00` | one timer, instance 0, id `0x000` |
| `CAN_TYPE_AUDIO` | `0x01` | one audio module, id `0x020` |
| `CAN_TYPE_WIRES` .. `CAN_TYPE_PASSWORD` | `0x10` .. `0x1A` | gameplay, negotiated instance |
| `CAN_TYPE_SERIAL_DISPLAY` | `0x20` | id `0x400` |
| `CAN_TYPE_INDICATOR_PANEL` | `0x21` | id `0x420` |
| `CAN_TYPE_BATTERY_HOLDER` | `0x22` | id `0x440` |
| `CAN_TYPE_PORT_PANEL` | `0x23` | id `0x460` |
| `CAN_TYPE_VENTING_GAS` | `0x30` | needy |
| `CAN_TYPE_CAPACITOR_DISCHARGE` | `0x31` | needy |
| `CAN_TYPE_KNOB` | `0x32` | needy |
| `CAN_TYPE_DEBUGGER` | `0x3C` | harness |
| `CAN_TYPE_DEBUGGER_SENDER` | `0x3D` | harness |
| `CAN_TYPE_BROADCAST` | `0x3F` | id `0x7E0`, timer fan-out |

`0x02`–`0x0F`, `0x1B`–`0x1F`, `0x24`–`0x2F`, `0x33`–`0x3B` are reserved. Add a new type in `can_bus.h` and in the `CAN_MODULE_TYPES` array. A duplicate value fails `static_assert`.

`decodeCanId`, `getModuleTypeName`, `getMessageTypeName` are for logs. `getModuleTypeName` only knows timer, audio, simon, serial display, and broadcast. Extend it when you add a type, or logs will say `UNKNOWN`.

`timer/lib/game_state/game_state.h` repeats side-module types as `0x40`+. Those numbers are not ids. Send `CAN_TYPE_SERIAL_DISPLAY`, not `ModuleType::SERIAL_DISPLAY`.

## Frame format

`sendCanMessage(receiverId, data, len)` queues the frame and returns. It does not wait for a free buffer or for the transceiver. `false` means the frame was not queued.

- `len` is 1..6. Longer calls are dropped.
- The sender id is written when a TX buffer is loaded, so a frame queued during id negotiation goes out with the final id.
- Latest `TIMER_TIME`, `TIMER_STRIKES`, `TIMER_COUNTDOWN`, `MODULE_HEARTBEAT`, and `MODULE_STATUS` replace an older queued copy of the same type and destination. Strikes, solved, register, game start/stop, and serial halves are kept ahead of heartbeats when the queue is full.
- A one-shot error (no ACK, arbitration loss) is retried up to three attempts from `handleCanMessages`. Nothing in this path calls `delay`.
- On the wire the MCP2515 DLC is `len + 2`. Bytes 0..1 are the sender's full id, big-endian. Bytes 2.. are `data`.
- The receiver callback is `void(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len)` where `id` is the destination id and `data` is already the caller's bytes. `data[0]` is the message type for every bomb message.
- Callbacks run only when the destination is `thisModuleId` or `CAN_ID_BROADCAST`. Register with `registerCanCallback` (max 8).
- `registerRawCanCallback` (max 2) sees every id, including other modules' traffic, with the same stripping. `can_debug_receive` uses the normal callback plus its own tracking. Use raw when you need frames that were not addressed to you.

`handleCanMessages()` drains the controller. Call it every loop on one core. It is not safe to call from both cores.

Id negotiation does not use `sendCanMessage`. `assignUniqueId` only arms a state machine; it returns as soon as the first probe is scheduled. `handleCanMessages` sends each raw 3-byte `[ID_PROBE, type, candidate]` to `CAN_INSTANCE_ID(type, 0)` and waits on `millis()`, so `loop` keeps running. A module that already holds that instance replies `[ID_TAKEN, type, instance]`. `ID_TAKEN` counts only when the instance matches the candidate being probed. Instance 0 is never a live gameplay id. Negotiation starts at instance 1, three probes, 200 ms plus jitter, then one confirmation probe. On total failure it keeps instance 1. `isCanIdReady()` is false until that finishes. `module_state` will not register or heartbeat before then.

```cpp
initCanBus(CAN_INSTANCE_ID(CAN_TYPE_WIRES, 0x00));
assignUniqueId(CAN_TYPE_WIRES);
// instance is valid only after isCanIdReady() becomes true inside loop
```

Fixed modules call `initCanBus(CAN_ID_AUDIO)` and do not negotiate. `assignUniqueId` returns false on the timer.

`updateCanId` stores the id used as the sender prefix and as the software filter. Hardware filters match the whole module type, so they stay put. `can_bus.cpp` calls `updateCanId`. Modules should not.

## Message types

Timer to modules, usually to `CAN_ID_BROADCAST`:

| Enum | Byte 0 | Rest of payload |
|---|---|---|
| `TIMER_GAME_START` | `0x10` | byte 1: `GameState` |
| `TIMER_GAME_STOP` | `0x11` | byte 1: `GameState` |
| `TIMER_STRIKES` | `0x12` | byte 1: strike count |
| `TIMER_SERIAL_NUMBER_FIRST_HALF` | `0x13` | 3 ASCII chars |
| `TIMER_SERIAL_NUMBER_LAST_HALF` | `0x14` | 3 ASCII chars |
| `TIMER_RESET` | `0x15` | |
| `TIMER_TIME` | `0x16` | bytes 1..4: remaining ms, `memcpy` of `unsigned long` |
| `TIMER_COUNTDOWN` | `0x17` | byte 1: seconds |
| `TIMER_MODULE_DISCOVERED` | `0x18` | sent to the module's own id, not broadcast |

Module to timer, destination `CAN_ID_TIMER`:

| Enum | Byte 0 | Notes |
|---|---|---|
| `MODULE_REGISTER` | `0x20` | until `TIMER_MODULE_DISCOVERED` |
| `MODULE_SOLVED` | `0x21` | |
| `MODULE_STRIKE` | `0x22` | |
| `MODULE_STATUS` | `0x23` | status, progress, solved flag. `module_state` fills this |
| `MODULE_HEARTBEAT` | `0x24` | |
| `MODULE_TELEMETRY` | `0x25` | |

`ModuleStatus`: `IDLE` 0, `ACTIVE` 1, `SOLVED` 2, `ARMED` 3, `ERROR` 0xFF.

`MODULE_TELEMETRY_FLAG` is `0x80` on a status byte. Telemetry subtypes: `MODULE_TELEMETRY_GENERAL` `0x40`, `CUSTOM0`..`CUSTOM2` `0x41`..`0x43`.

Audio, destination `CAN_ID_AUDIO`. One code per byte, and `module_state->playAudio` can pack several:

| Enum | Value |
|---|---|
| `AUDIO_PLAY_TITLE` | `0x40` |
| `AUDIO_PLAY_BEEP_NORMAL` / `FAST` / `HIGH` | `0x41` / `0x42` / `0x43` |
| `AUDIO_PLAY_STRIKE` | `0x44` |
| `AUDIO_PLAY_DEFUSED` | `0x45` |
| `AUDIO_PLAY_EXPLODED` | `0x46` |
| `AUDIO_PLAY_CORRECT_TIME` | `0x47` |
| `AUDIO_PLAY_GAME_OVER_FANFARE` | `0x48` |
| `AUDIO_PLAY_ALARM_CLOCK_BEEP` / `SNOOZE` | `0x49` / `0x4A` |
| `AUDIO_PLAY_ALARM_EMERGENCY` | `0x4B` |
| `AUDIO_PLAY_SIMON_RED` / `GREEN` / `YELLOW` / `BLUE` | `0x4C` .. `0x4F` |
| `AUDIO_SET_VOLUME` | `0x60`, byte 1 is percent 0..100 |

Serial display, destination `CAN_ID_SERIAL_DISPLAY`:

| Enum | Value | Payload |
|---|---|---|
| `SERIAL_DISPLAY_SET_SERIAL` | `0x30` | 6 ASCII chars. Needs 7 bytes, which does not fit in `sendCanMessage`. The live path is the timer's two serial-half messages, reassembled by `module_state`. |
| `SERIAL_DISPLAY_CLEAR` | `0x31` | |
| `SERIAL_DISPLAY_SHOW_CREDIT` | `0x32` | |

Id negotiation, raw frames, not callbacks: `ID_PROBE` `0x01`, `ID_TAKEN` `0x02`.

## Connection flags

`updateModuleConnections` clears `audioModuleConnected` and `serialDisplayConnected` after 5 s (`MODULE_TIMEOUT_MS`). Those two booleans are updated from message traffic inside `can_bus.cpp`. New modules should track presence on the timer through heartbeats in `game_state`, not by adding more globals here.

## Logging

`logCanMessage` prints direction, ids, type name, and bytes when `senderId != 0`. Automatic TX/RX logs are off. Call `setCanBusLogging(true)` to queue them; `handleCanMessages` prints at most one line per call so the UART cannot stall the MCP2515. Negotiation frames are not logged. Drops and RX overflows print at most once a second. `getCanBusCounters()` returns delivered, overflow, retry, and drop counts.
