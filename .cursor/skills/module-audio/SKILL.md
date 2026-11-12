---
name: module-audio
description: >-
  Audio module (support/audio/): Pico W, TLV320DAC3100, I2S mixer, SD sound bank.
  Real-life bomb audio, not a manual puzzle. Use when editing support/audio/, adding
  a sound, changing volume, or moving work between the two cores.
---

# Audio module

`support/audio/` on a Pico W (`board = rpipicow`). Fixed id `CAN_ID_AUDIO`. `initModuleState(MODULE_STATE_NO_LED)`. Not a solvable module. Other modules request sounds; this one plays them.

`-DEDGE_AUDIO` selects the two-core scheduler. `-DPIO_FRAMEWORK_ARDUINO_ENABLE_BLUETOOTH` compiles the Bluetooth stack. `lib/bluetooth/` can scan and open an A2DP source, and `main.cpp` never calls it. Playback is I2S into the TLV320.

## Pins

| Function | GPIO |
|---|---|
| I2S BCLK, WSEL, DIN | GP3, GP4, GP5 |
| Codec I2C `Wire1` SDA, SCL | GP6, GP7 |
| Codec reset, active low | GP8 |
| SD SPI1 MISO, CS, SCK, MOSI | GP12, GP13, GP14, GP15 |
| CAN SPI0 | CS GP17, INT GP20 |
| Console | GP0 / GP1 |

## Boot order

From `src/main.cpp`: console, `initCanBus(CAN_ID_AUDIO)`, callback, `initModuleState`, `initAudioMixer()` and wait until `audioMixerReady()`, then `initSdCard()` and `initSoundBank()`. Mixer before SD, because codec I2C and the first I2S clocks are easier to debug before SPI1 is active. Inside the mixer, the codec order is phase-1, `I2SStream.begin`, phase-2. See [adafruit-tlv320](../adafruit-tlv320/SKILL.md) and [arduino-audio-tools](../arduino-audio-tools/SKILL.md).

## Cores when EDGE_AUDIO is set

Core 0 `loop` calls only `updateAudioMixer()`. Core 1 `loop1` calls `handleCanMessages`, `updateModuleState`, `processOnePendingAudioPlay`, and `audioStreamFeederPump`.

`ktaneAudioBlockingYield` is called from long SD reads so CAN still runs on core 1. SdFat calls take `audioSdLock()`.

Without `EDGE_AUDIO`, one `loop` interleaves eight mixer updates with CAN and one pending sound. Use that only on the bench.

## How a sound is chosen

CAN callback: `moduleStateHandleCanMessage` first, then if `id == CAN_ID_AUDIO`:

- `AUDIO_SET_VOLUME` (`0x60`) plus a percent byte calls `setAudioMixerVolume`. This scales mixed PCM. It does not rewrite the codec PGA.
- Any other known `AudioMessage` is queued (8 deep) and played from core 1 one at a time.

`getSoundFromBank`:

- `Success`: PCM fits in RAM, `playSound(samples, count)`.
- `FileTooLarge`: `playSoundFromFile(path)` streams it.
- `SoundNotRegistered` / open / read errors: log and drop.

Files, under the volume root, from `sound_bank.cpp`:

| Message | File |
|---|---|
| `AUDIO_PLAY_TITLE` | `/sounds/title.wav` |
| `AUDIO_PLAY_BEEP_NORMAL` | `/sounds/double_beep.wav` |
| `AUDIO_PLAY_BEEP_FAST` | `/sounds/double_beep_125.wav` |
| `AUDIO_PLAY_BEEP_HIGH` | `/sounds/time_beep_high.wav` |
| `AUDIO_PLAY_STRIKE` | `/sounds/strike.wav` |
| `AUDIO_PLAY_DEFUSED` | `/sounds/bomb_defused.wav` |
| `AUDIO_PLAY_EXPLODED` | `/sounds/explosion.wav` |
| `AUDIO_PLAY_CORRECT_TIME` | `/sounds/correct_digital_chime.wav` |
| `AUDIO_PLAY_GAME_OVER_FANFARE` | `/sounds/game_over_fanfare.wav` |
| `AUDIO_PLAY_ALARM_CLOCK_BEEP` | `/sounds/alarm_clock_beep.wav` |
| `AUDIO_PLAY_ALARM_CLOCK_SNOOZE` | `/sounds/alarm_clock_snooze.wav` |
| `AUDIO_PLAY_ALARM_EMERGENCY` | `/sounds/alarm_emergency.wav` |
| `AUDIO_PLAY_SIMON_RED/GREEN/YELLOW/BLUE` | `/sounds/simon_red.wav` and the matching color |

WAV is 16-bit PCM at 44100 Hz. The mixer rejects other rates. `lib/sounds/sound_headers/` holds older embedded PCM headers (`sounds.h`). The live bank reads the card. Add a sound by dropping a file on the card and adding a `SOUND_ENTRY` plus a name in `getSoundName`. Add the enum in `can_bus.h` first if it is a new message.

## Mixer

`audio_mixer.h`: `initAudioMixer`, `updateAudioMixer`, `playSound` (sample count, not bytes), `playSoundFromFile`, `audioMixerReady`, `setAudioMixerVolume` / `getAudioMixerVolume` (0..100, default 20).

`EDGE_AUDIO` crosses cores with a lock-free IPC queue (`audio_mixer_ipc.h`): RAM clips, volume, and stream-slot arm/stop. Six RAM voices. Stream slots are filled by `stream_feeder` on core 1 and consumed on core 0. Do not call `playSound` from core 0 under `EDGE_AUDIO`; the feeder and the pending-play path already hop the queue.

`lib/sd_test/` can list the card. It is a helper, not part of boot.

## Status LED and game state

No status LED. The callback sets `MODULE_STATUS_ACTIVE` around a play and `IDLE` after. The module still heartbeats so the timer can show it connected.
