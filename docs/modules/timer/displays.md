# Timer displays

Both displays sit on `Wire` after `Wire.setSDA(8)` and `Wire.setSCL(9)`.

## Countdown, address `0x70`

`timer/lib/countdown/countdown.cpp`. `Adafruit_7segment`, four digits, positions `{0, 1, 3, 4}` so the colon column is skipped.

While `gameState.isRunning()`:

- The digits show `minutes * 100 + seconds` through `display.print`.
- The colon toggles every 500 ms, or every 125 ms once the strike count is 2 or more.
- Once per new second the timer sends one byte to `CAN_ID_AUDIO`:
  - 0 strikes: `AUDIO_PLAY_BEEP_NORMAL`
  - 1 strike: `AUDIO_PLAY_BEEP_FAST`
  - 2 strikes: `AUDIO_PLAY_BEEP_HIGH`
- While `isEmergencyTime()` is true (remaining time at or under 60 seconds, and still above 0), it also sends `AUDIO_PLAY_ALARM_EMERGENCY` every 3 seconds.

When the game is not running, `updateCountdownDisplay` calls `updateCountdownRaw` with a four-character banner:

| State | String passed in |
|---|---|
| `IDLE` | `----` |
| `PAUSED` | `\|\|` |
| `EXPLODED` | `BOOM` |
| `DEFUSED` | `SAFE` |
| anything else | `----` |

`updateCountdownRaw` has glyphs for `-` (segment bit `0x40`) and for digits `0`–`9`. Any other character writes a blank raw digit. `----` shows as dashes. `BOOM`, `SAFE`, and `||` currently clear those positions rather than drawing letters. The Adafruit 7-segment font can draw a limited alphabet through `writeDigitAscii` if you want those banners to be readable. The running clock path does not use `updateCountdownRaw`.

## Strikes, address `0x74`

`timer/lib/strikes/strikes.cpp` does not use `Adafruit_AlphaNum4`. `initStrikeDisplay` writes three HT16K33 commands:

| Byte | Meaning |
|---|---|
| `0x21` | oscillator on |
| `0x81` | display on, no blink |
| `0xEF` | brightness max (`0xE0 \| 0x0F`) |

`getCustomChar('X')` returns `0x280A` (segments chosen to look like the manual's strike X). `updateAlphaDisplay` writes that pattern to digit registers 2 and 1 (left, then right).

| Strikes | Glass |
|---|---|
| 0 | blank |
| 1 | one X, steady |
| 2 or more | two X characters, toggled every 125 ms |

The strike sound is not sent from this file. `onStrikeChange` in `timer/src/main.cpp` sends `AUDIO_PLAY_STRIKE` when the new count is non-zero, and broadcasts `TIMER_STRIKES`.
