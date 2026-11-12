---
name: ktane-module-state
description: >-
  Register a KTANE module with the timer using shared_libs/module_state:
  discovery, heartbeat, status LED, strikes, serial number, edgework, and
  playAudio. Use when writing any module except the timer, or when editing
  module_state. The timer must not call initModuleState.
---

# module_state

`shared_libs/module_state/`. Include `<module_state.h>` after `<can_bus.h>`. Every module except the timer owns one `ModuleState`. The timer tracks modules in `timer/lib/game_state/` instead. Calling `initModuleState` on the timer would make the master try to register with itself.

The singleton is `globalModuleState`, created by `initModuleState`. You can also stack-allocate a `ModuleState` and call `begin`. Do not do both in one sketch. Simon, audio, and serial display use the singleton.

## Setup and loop

```cpp
initCanBus(CAN_INSTANCE_ID(CAN_TYPE_WIRES, 0x00));
assignUniqueId(CAN_TYPE_WIRES);
registerCanCallback(onCanMessage);
initModuleState(STATUS_LED_PIN);   // or MODULE_STATE_NO_LED

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  moduleStateHandleCanMessage(id, senderId, data, len);
  // module-specific commands after that
}

void loop() {
  handleCanMessages();
  updateModuleState();
}
```

`moduleStateHandleCanMessage` must run or the module never leaves discovery and never learns the serial number. It consumes timer broadcasts. It does not consume audio or serial-display commands; those modules inspect `data[0]` themselves after the call.

Define `MODULE_STATE_DEFAULT_LED_PIN` before the include to change the default. The header defaults it to `MODULE_STATE_NO_LED` (`-1`).

## What update() does

`updateModuleState()` / `ModuleState::update()`:

- Does not register, heartbeat, or send status until `isCanIdReady()`. `assignUniqueId()` returns before the instance is chosen; the first register goes out on the first `updateModuleState()` after negotiation finishes.
- Sends `MODULE_REGISTER` every `MODULE_STATE_REGISTER_INTERVAL` (1 s) until the timer replies `TIMER_MODULE_DISCOVERED`.
- Sends `MODULE_HEARTBEAT` every `MODULE_STATE_HEARTBEAT_INTERVAL_DISCOVERY` (3 s) until the game is running, then every `MODULE_STATE_HEARTBEAT_INTERVAL_GAME` (1 s).
- Flashes the status LED 500 ms on / 500 ms off until discovered, then off.
- Flashes the LED for `MODULE_STATE_STRIKE_FLASH_DURATION` (1 s) when the strike count changes.
- Sends `MODULE_STATUS` when status, progress, or solved is dirty.

Those millisecond constants in the header are the ones that run. Older README text that says "1 s discovery, 5 s in game" is stale.

## Status API

```cpp
setModuleStateStatus(MODULE_STATUS_ACTIVE);  // IDLE, ACTIVE, SOLVED, ARMED, ERROR
setModuleStateProgress(0..100);
setModuleStateSolved(true);
```

Equivalent methods on the object: `setStatus`, `setProgress`, `setSolved`, `getStatus`, `getProgress`, `isSolved`.

`MODULE_STATUS_ARMED` is for needy modules. `MODULE_STATUS_SOLVED` should agree with `setSolved(true)`. Simon updates both from its phase enum, and only writes when the value changed.

## Timer state you can read

After the matching broadcast has arrived:

```cpp
globalModuleState->isGameRunning();
globalModuleState->isDiscoveredByTimer();
globalModuleState->getSerialNumber();     // 6 chars, both halves
globalModuleState->getStrikeCount();
globalModuleState->getBatteryCount();
globalModuleState->getEdgework();         // indicators, ports, batteries
```

Edgework helpers on `Edgework`: `hasIndicator`, `hasLitIndicator`, `hasUnlitIndicator`, `hasPort`, `getLitIndicatorCount`, `getUnlitIndicatorCount`, `getPortCount`. Indicator labels match the manual: `SND CLR CAR IND FRQ SIG NSA MSA TRN BOB FRK`. Ports: `PARALLEL`, `SERIAL_PORT`, `PS2`, `RJ45`, `RCA`, `DVI`, `STEREO_RCA`.

The timer does not currently broadcast a full edgework blob on every game. `getEdgework()` stays empty until something calls `setEdgework` or a future timer message fills it. Write puzzle rules against the accessors anyway, so they start working when the timer sends the data. Serial number and strikes do arrive today (`TIMER_SERIAL_NUMBER_*`, `TIMER_STRIKES`).

## Callbacks

```cpp
globalModuleState->setGameStateCallback([](bool running) {});
globalModuleState->setStrikeCallback([](uint8_t strikes) {});
globalModuleState->setSerialNumberCallback([](const String& serial) {});
globalModuleState->setEdgeworkCallback([](const Edgework& ew) {});
globalModuleState->setDiscoveredCallback([]() {});
```

The function pointers are `GameStateCallback`, `StrikeCallback`, `SerialNumberCallback`, `EdgeworkCallback`, `DiscoveredCallback`. No user data pointer. Use a file-scope object, as Simon does.

## Strikes, audio, extra frames

```cpp
globalModuleState->triggerStrike();                 // MODULE_STRIKE to the timer
globalModuleState->playAudio(AUDIO_PLAY_SIMON_RED);
uint8_t chord[] = {AUDIO_PLAY_BEEP_NORMAL, AUDIO_PLAY_BEEP_HIGH};
globalModuleState->playAudio(chord, 2);             // up to 6 codes
globalModuleState->sendTimerMessage(data, len);     // len <= 6
globalModuleState->sendBroadcastMessage(data, len);
globalModuleState->sendMessage(receiverId, data, len);
globalModuleState->sendTelemetry(MODULE_TELEMETRY_CUSTOM0, payload, len);
```

`playAudio` writes to `CAN_ID_AUDIO`. Simon's `sendAudio` calls `sendCanMessage` itself; both are valid. Prefer `playAudio` in new modules so a muted bus (`setCommunicationEnabled(false)`) suppresses it.

`setCommunicationEnabled(false)` stops register, heartbeat, and status traffic. The manual setters `setDiscovered`, `setGameRunning`, `setStrikeCount`, `setSerialNumber`, `setEdgework` exist for a bench test with no timer. Leave them unused on a module that is on the bus, or the LED and the real timer will disagree.

## LED

```cpp
setModuleStateLedPin(11);
setModuleStateLedState(true);     // manual override, solid
clearModuleStateLedOverride();    // back to discovery / strike / off
disableModuleStateLed();
```

Solved-solid is not automatic. Simon leaves the status LED to the library and shows solved on the four color LEDs. Call `setLedState(true)` from the solved callback if this module's status LED should stay lit.

`sendRegisterNow` and `sendHeartbeatNow` force a frame. Useful in a debug command, not in `loop`.
