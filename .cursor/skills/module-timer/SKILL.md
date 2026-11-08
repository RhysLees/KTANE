---
name: module-timer
description: >-
  Timer module (timer/): KTANE master on a Pico W. Countdown, strikes,
  game state, CAN fan-out, and the WiFi setup UI. Use when editing timer/,
  GameStateManager, the countdown, strikes, or the web API. This module
  does not use module_state.
---

# Timer module

`timer/` on a Pico W (`board = rpipicow`). It is the only master. It broadcasts game state and collects `MODULE_*` frames. It does not call `initModuleState`.

`src/main.cpp` order: console, `Wire` on GP8/GP9, `initCanBus(CAN_ID_TIMER)`, strike display, countdown, `GameStateManager` config, `initWebServer`. `loop` is `gameState.tick()`, refresh both HT16K33s, `handleCanMessages()`, `updateWebServer()`.

Build flag `-DMODULE_TIMER` is set and not read by current sources. `-DKTANE_PICOPROBE_UART` is the console.

## Hardware

| Device | Bus | Address or pins |
|---|---|---|
| Countdown, `Adafruit_7segment` | `Wire` I2C0 SDA GP8 SCL GP9 | `0x70` |
| Strikes, raw HT16K33 | same `Wire` | `0x74` |
| CAN | SPI0 | CS GP17, INT GP20 |
| Console | `Serial1` | GP0 TX, GP1 RX |

GP0/GP1 stay free for the probe. Do not move I2C back to the Pico default GP4/GP5 without a reason; the comment in `setupHardware` records why it is GP8/GP9.

Countdown: [adafruit-led-backpack](../adafruit-led-backpack/SKILL.md). `updateCountdownDisplay` prints `minutes * 100 + seconds`, blinks the colon (500 ms, or 125 ms at 2 strikes), and sends `AUDIO_PLAY_BEEP_NORMAL` / `FAST` / `HIGH` once per second. Under 60 s it also sends `AUDIO_PLAY_ALARM_EMERGENCY` every 3 s. Idle shows `----`, exploded `BOOM`, defused `SAFE`, paused `||`.

Strikes (`timer/lib/strikes/`): not the Adafruit alpha class. `initStrikeDisplay` sends HT16K33 commands `0x21` oscillator, `0x81` display on, `0xEF` brightness. `getCustomChar('X')` is `0x280A`. `updateAlphaDisplay` writes digits 2 and 1. One X per strike, blinking.

## GameStateManager

`timer/lib/game_state/`. One global in `main.cpp`. Default config in `setupGameConfig`: 5 minutes, 3 strikes, strike acceleration 0.25, emergency at 60 s, needy modules and edgework enabled.

States: `IDLE`, `RUNNING`, `PAUSED`, `EXPLODED`, `DEFUSED`, `VICTORY`. `tick()` advances the clock, needy modules, timeouts, and win/lose checks. `startGame()` is the entry that tells modules and then runs the clock.

Callbacks wired in `main.cpp`:

- State change broadcasts `TIMER_GAME_START` or `TIMER_GAME_STOP` with the `GameState` byte, and plays exploded / defused / fanfare.
- Strike change broadcasts `TIMER_STRIKES` and plays `AUDIO_PLAY_STRIKE` when the count is non-zero.
- A solved module plays `AUDIO_PLAY_CORRECT_TIME`.
- Time callback broadcasts `TIMER_TIME` (type byte plus 4-byte `unsigned long` milliseconds) at 2 Hz.

`onTimerCanMessage` handles `MODULE_REGISTER`, `MODULE_STRIKE`, `MODULE_SOLVED`, `MODULE_STATUS`, `MODULE_HEARTBEAT` by calling `gameState.handleCanMessage`. Anything else is ignored.

Side-module enum values in this header (`SERIAL_DISPLAY = 0x40` and neighbors) do not match `can_bus.h` (`0x20` and neighbors). Outbound ids must use `CAN_ID_*` from `can_bus.h`.

Public surface worth using from the web API: `startGame`, `pauseTimer`, `resumeTimer`, `reset`, `addStrike`, `clearStrikes`, `setTimeLimit`, `setStrikes`, `getState`, `getRemainingTime`, `getStrikes`, `getAllModules`, `getSerialNumber`, `getEdgework`, `setAudioVolume`. `initialize()` builds the serial and edgework for the round.

Heartbeat timeouts live at the top of `game_state.h`: discovery 6 s, in-game 2 s, inactive 5 s. They are sized for the intervals in `module_state.h` (3 s then 1 s).

## WiFi UI

`timer/lib/web_server/`. Pico W AP, SSID `KTANE_TIMER`, password `ktane12345`, `WiFiServer` on port 80. `updateWebServer()` must stay on core 0. JSON bodies are ArduinoJson 6. See [arduinojson](../arduinojson/SKILL.md) and [arduino-pico](../arduino-pico/SKILL.md). Credentials can be stored with `EEPROM.begin` / `put` / `commit`.

`web_html.cpp` is the page. `web_api.cpp` is the route handlers. `web_utils.cpp` is shared formatting. Add a route in `web_api.cpp` and call it from the request switch in `web_server.cpp`. Keep documents short-lived and on the heap (`DynamicJsonDocument`).

## Libraries compiled in, not started

`timer/lib/lcd1602/` is a Waveshare RGB1602 driver: character controller address `(0x7C >> 1)` = `0x3E`, RGB controller `(0xC0 >> 1)` = `0x60`, on a `TwoWire` you pass in. `initLcd1602`, `lcd1602PrintLine`, `lcd1602SetColor`, `lcd1602SetColorRGB`. `timer/lib/debug/debug.cpp` draws menus on it. `main.cpp` does not call `initLcd1602` or the debug menu. `serial_command/` and `module_tracker/` are also present and not referenced from `main.cpp`. Wire them from `setup` if you revive that UI; do not assume they run today.

## Adding timer behavior

Play a sound with one byte to `CAN_ID_AUDIO`. Tell every module with `sendCanMessage(CAN_ID_BROADCAST, ...)`. Tell one module by using the id it registered with. Stay at or under 6 payload bytes. Serial number is already split into `TIMER_SERIAL_NUMBER_FIRST_HALF` and `TIMER_SERIAL_NUMBER_LAST_HALF` because a 6-character serial plus a type byte does not fit in one prefixed frame.
