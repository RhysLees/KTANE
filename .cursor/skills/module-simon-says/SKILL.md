---
name: module-simon-says
description: >-
  Simon Says gameplay module (simon_says/): buttons, color LEDs, vowel and
  strike mapping, CAN audio. Use when editing simon_says/ or copying it as
  the template for a new puzzle module.
---

# Simon Says

`simon_says/` on a Pico (`board = rpipico`). Reference gameplay module: dynamic CAN id, `module_state`, puzzle rules from the manual, sounds sent to the audio module.

`src/main.cpp` owns the bus. `lib/simon_says/` owns the puzzle and the GPIO. The README's pin table and its "starts at 3 colors" line disagree with the header. Trust `simon_says.h`.

## Pins

Status LED GP11, passed to `initModuleState`.

`COLOR_ORDER` is red, blue, green, yellow. `LED_PINS` and `BUTTON_PINS` follow that order, not the order of the `*_PIN_*` constants.

| Color | Button (INPUT_PULLUP, pressed = LOW) | LED (HIGH = on) |
|---|---|---|
| Red | GP2 | GP6 |
| Blue | GP4 | GP8 |
| Green | GP3 | GP7 |
| Yellow | GP5 | GP9 |

`begin()` drives every LED high once before `update()` takes over. `setColorLed` uses HIGH for on.

Debounce is 30 ms. Display flash is 500 ms, gap 200 ms, input timeout 5 s, strike flash 1 s.

## Rules implemented

`SIMON_INITIAL_SEQUENCE` is 1. Each solved stage appends one color, up to `SIMON_MAX_SEQUENCE_LENGTH` 5. Progress percent is stages completed over that span. A wrong press calls `registerStrike` (local count plus `AUDIO_PLAY_STRIKE`) and `main.cpp` turns that into `triggerStrike()` on the rising edge of `consumeStrikeTriggered()`. Timing out a stage is not a strike. Official mapping tables:

Vowel in the serial (`serialHasVowel`): flash red/blue/green/yellow maps to press blue/red/yellow/green at 0 strikes, yellow/green/blue/red at 1, green/red/yellow/blue at 2+.

No vowel: blue/yellow/green/red, then red/blue/yellow/green, then yellow/green/blue/red.

`mapFlashToPress` indexes `[strikeLevel 0..2][color]`. Strike level clamps at 2. The serial callback stores the string; an empty serial takes the consonant table.

## Sounds

`sendAudio` is `sendCanMessage(CAN_ID_AUDIO, {code}, 1)`.

Display flashes use `AUDIO_PLAY_SIMON_RED/BLUE/GREEN/YELLOW` in color order. Button presses use `AUDIO_PLAY_BEEP_NORMAL`, `AUDIO_PLAY_CORRECT_TIME`, `AUDIO_PLAY_BEEP_HIGH`, `AUDIO_PLAY_BEEP_FAST` in that same color order. Solved sends `AUDIO_PLAY_DEFUSED`.

## Phases

`Idle`, `Displaying`, `Input`, `StrikeFlash`, `Solved`. `main.cpp` maps Idle to `MODULE_STATUS_IDLE`, Solved to `MODULE_STATUS_SOLVED`, anything else to `MODULE_STATUS_ACTIVE`, and pushes progress and solved only when they change.

`onGameStateChanged(false)` resets status, solved, and progress. `onGameStateChanged(true)` marks the module active. Serial and strike callbacks forward into `simonSays.setSerialNumber` / `setStrikeCount`.

## Copying this for a new puzzle

1. New top-level directory, `platformio.ini` cloned from this one, `build_dir` and `default_envs` renamed. Keep `lib_extra_dirs = ../shared_libs` and `mcp_can`.
2. New `CAN_TYPE_*` in `shared_libs/can_bus/can_bus.h` if one is not already reserved. Simon's type is `CAN_TYPE_SIMON` (`0x13`).
3. `main.cpp` can stay almost identical: `assignUniqueId`, one CAN callback that only calls `moduleStateHandleCanMessage`, callbacks into the puzzle object, `triggerStrike` on failure, `setModuleStateSolved` on success.
4. Read the rule from `DOCS/KeepTalkingAndNobodyExplodes-BombDefusalManual-v1.pdf` and from `getSerialNumber()`, `getStrikeCount()`, `getEdgework()`.
5. Do not include `module_state` inside the puzzle library if that creates an include cycle. Simon's library includes `can_bus.h` only, and `main.cpp` includes `module_state.h`. Follow that split.
