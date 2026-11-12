# module_state library

`shared_libs/module_state/`. Include `<module_state.h>` after `<can_bus.h>`. Every module except the timer owns one `ModuleState`. The timer tracks modules in `timer/lib/game_state/` instead.

The lifecycle diagram and the timing table are in [Module lifecycle](../architecture/module-lifecycle.md). This page is the API a puzzle sketch calls.

Simon, audio, and the serial tag use the singleton from `initModuleState`. You can stack-allocate a `ModuleState` and call `begin`. Do not do both in one sketch.

## Setup

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

`moduleStateHandleCanMessage` consumes timer broadcasts. It does not consume audio or serial-display commands. Those sketches inspect `data[0]` after the call. If you skip the call, the module never leaves discovery and never learns the serial.

Define `MODULE_STATE_DEFAULT_LED_PIN` before the include to change the default. The header defaults it to `MODULE_STATE_NO_LED` (`-1`).

## What update() sends

Nothing, until `isCanIdReady()` is true. After that:

- `MODULE_REGISTER` every 1 s until `TIMER_MODULE_DISCOVERED`.
- `MODULE_HEARTBEAT` every 3 s until the game is running, then every 1 s.
- Status LED 500 ms on / 500 ms off until discovered, then off.
- Status LED for 1 s when the strike count changes.
- `MODULE_STATUS` when status, progress, or solved is dirty.

`sendRegisterNow` and `sendHeartbeatNow` force a frame. Use them from a debug command, not from `loop`.

## Publish puzzle state

```cpp
setModuleStateStatus(MODULE_STATUS_ACTIVE);  // IDLE, ACTIVE, SOLVED, ARMED, ERROR
setModuleStateProgress(0..100);
setModuleStateSolved(true);
```

The same methods exist on the object: `setStatus`, `setProgress`, `setSolved`, `getStatus`, `getProgress`, `isSolved`.

`MODULE_STATUS_ARMED` is for needy modules. `MODULE_STATUS_SOLVED` should agree with `setSolved(true)`. Write them only when the value changed, as Simon does.

Solved-solid is not automatic. Call `setLedState(true)` if this board's status LED should stay lit after a solve. `setLedState(true)` is a manual override. `clearModuleStateLedOverride()` returns the pin to discovery / strike / off.

## Read timer state

```cpp
globalModuleState->isGameRunning();
globalModuleState->isDiscoveredByTimer();
globalModuleState->getSerialNumber();     // 6 chars, both halves
globalModuleState->getStrikeCount();
globalModuleState->getBatteryCount();
globalModuleState->getEdgework();
```

Serial and strikes arrive today. `getEdgework()` stays empty until something calls `setEdgework` or a future timer message fills it. Indicator labels and port names match the paper manual. See [Module lifecycle](../architecture/module-lifecycle.md).

Callbacks are function pointers with no user-data argument: `setGameStateCallback`, `setStrikeCallback`, `setSerialNumberCallback`, `setEdgeworkCallback`, `setDiscoveredCallback`. Use a file-scope object.

## Strikes, audio, extra frames

```cpp
globalModuleState->triggerStrike();                 // MODULE_STRIKE to the timer
globalModuleState->playAudio(AUDIO_PLAY_SIMON_RED); // one byte to CAN_ID_AUDIO
globalModuleState->sendTimerMessage(data, len);     // len <= 6
globalModuleState->sendBroadcastMessage(data, len);
globalModuleState->sendMessage(receiverId, data, len);
globalModuleState->sendTelemetry(MODULE_TELEMETRY_CUSTOM0, payload, len);
```

`playAudio(const uint8_t*, len)` will put several codes in one frame. The audio module's callback reads `data[0]` only, then returns. Send one code per frame if each sound has to be heard.

`setCommunicationEnabled(false)` stops register, heartbeat, and status traffic, and `playAudio` goes through the same gate. Simon's `sendAudio` calls `sendCanMessage` itself, so that mute switch does not cover it.

`setDiscovered`, `setGameRunning`, `setStrikeCount`, `setSerialNumber`, and `setEdgework` exist for a bench with no timer. Leave them unused on a module that is on the bus.
