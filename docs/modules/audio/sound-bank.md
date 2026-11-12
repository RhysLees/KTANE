# Sound bank

The live bank is the SD card. `audio/lib/sounds/sound_headers/` holds older embedded PCM (`sounds.h`). Boot does not play those headers.

Paths come from `SOUND_ENTRY` lines in `audio/lib/sound_bank/sound_bank.cpp`. They are absolute on the volume, with a leading slash.

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
| `AUDIO_PLAY_SIMON_RED` | `/sounds/simon_red.wav` |
| `AUDIO_PLAY_SIMON_GREEN` | `/sounds/simon_green.wav` |
| `AUDIO_PLAY_SIMON_YELLOW` | `/sounds/simon_yellow.wav` |
| `AUDIO_PLAY_SIMON_BLUE` | `/sounds/simon_blue.wav` |

## Format

| Field | What the firmware accepts |
|---|---|
| Container | WAV, PCM |
| Sample width | 16-bit |
| Channels | mono or stereo |
| Rate, streamed | 44100 Hz. Anything else returns false from `playSoundFromFile`. |
| Rate, loaded to RAM | The bank warns unless the rate is 44100, 22050, or 48000, then still returns samples. The I2S clock stays 44100, so ship 44100. |

Short UI beeps load into RAM (`SoundBankStatus::Success`) and play on one of six mixer voices. A file that does not fit returns `FileTooLarge` and is streamed from core 1 into a ring that core 0 mixes. `length` passed to `playSound` is a sample count, not a byte count.

Names for the probe log live in `getSoundName` in `audio/src/main.cpp`. `AUDIO_PLAY_CORRECT_TIME` has a file and no dedicated name there, so a queued chime can log as `Unknown sound` even when it plays. Add a case if you want the log to match.

## Add a sound

1. Add an `AudioMessage` value in `shared_libs/can_bus/can_bus.h` if it is a new code. Keep it a single byte. `AUDIO_SET_VOLUME` (`0x60`) is not a file.
2. Drop a 44100 Hz 16-bit PCM WAV on the card.
3. Add a `SOUND_ENTRY` in `sound_bank.cpp` and a name in `getSoundName`.
4. Send that byte to `CAN_ID_AUDIO` from the module that should play it, or call `globalModuleState->playAudio(...)`.

`playAudio` can pack up to six codes in one frame. The audio callback queues `data[0]` only when it is handling a single-byte play, and handles volume when `data[0]` is `AUDIO_SET_VOLUME`. A packed chord from `playAudio` is several codes in one payload. Check `module_state` before relying on chords: the audio `onCanMessage` reads `data[0]` and, unless that byte is volume, queues that one id. Extra bytes in the same frame are not walked. Send one code per frame if the sound must be heard.

## Who sends what

| Source | Sounds |
|---|---|
| Timer countdown, each new second | normal, fast, or high beep from the strike count |
| Timer countdown, under 60 s | `AUDIO_PLAY_ALARM_EMERGENCY` every 3 s |
| Timer state callback | explosion, defused, fanfare |
| Timer strike callback | `AUDIO_PLAY_STRIKE` when the count is non-zero |
| Timer module-solved callback | `AUDIO_PLAY_CORRECT_TIME` |
| Simon display | one Simon color WAV per flash |
| Simon press | beep / chime / high / fast, by color |
| Simon solved | `AUDIO_PLAY_DEFUSED` |

`AUDIO_PLAY_TITLE`, the alarm-clock beep, and the snooze sound have files and no current caller in the timer or Simon.
