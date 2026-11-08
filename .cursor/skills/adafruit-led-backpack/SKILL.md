---
name: adafruit-led-backpack
description: >-
  Drive the HT16K33 7-segment countdown on the timer with Adafruit LED
  Backpack (Adafruit_7segment). Use when editing timer/lib/countdown, adding
  another HT16K33 backpack, or calling writeDigitNum, drawColon, or
  writeDisplay. The strike display is raw Wire, not this library.
---

# Adafruit LED Backpack

`adafruit/Adafruit LED Backpack Library`, plus GFX (already in the timer `lib_deps`). Include `<Adafruit_LEDBackpack.h>` and `<Wire.h>`.

The countdown in `timer/lib/countdown/countdown.cpp` is an `Adafruit_7segment` at I2C address `0x70` on `Wire` (I2C0, SDA GP8, SCL GP9).

The strike "XX" display at `0x74` is not this library. `timer/lib/strikes/strikes.cpp` writes HT16K33 registers itself because the glyph is a custom 14-segment pattern (`0x280A`). Use the backpack classes for a normal 7-segment, 14-segment, or matrix. Keep the strike writer as it is.

## Bring-up

`Wire.begin()` must run before `display.begin()`. The timer does that in `setupHardware()`.

```cpp
#include <Wire.h>
#include <Adafruit_LEDBackpack.h>

Adafruit_7segment countdown;

void setup() {
  Wire.setSDA(8);
  Wire.setSCL(9);
  Wire.begin();
  if (!countdown.begin(0x70, &Wire)) {
    // no ACK: address jumper, SDA/SCL, or 5 V vs 3.3 V pull-ups
  }
}
```

`begin(uint8_t addr = 0x70, TwoWire* wire = &Wire)` turns the oscillator on, clears RAM, sets blink off and brightness 15. Address jumpers select `0x70`–`0x77`. Passing `&Wire` matters, because the library default is also `&Wire` but a future move to `Wire1` will silently talk to the wrong block if you omit it.

## 7-segment geometry

Digit positions are 0, 1, 3, 4. Position 2 is the colon. `print` skips position 2 on its own.

```cpp
countdown.clear();
countdown.print(1234);             // integer, or print(0xBEEF, HEX), or print(12.34)
countdown.writeDisplay();

countdown.writeDigitNum(0, 1);     // position, value 0-15, optional decimal point
countdown.writeDigitNum(1, 2, true);
countdown.drawColon(true);
countdown.writeDigitNum(3, 3);
countdown.writeDigitNum(4, 0);
countdown.writeDisplay();

countdown.writeDigitAscii(0, 'A');
countdown.writeDigitRaw(0, 0x40);  // middle segment only: the dash used for "----"
countdown.writeDisplay();
```

Drawing functions edit a local buffer. Nothing changes on the glass until `writeDisplay()`. `drawColon` only sets that buffer; `writeColon()` exists to flush the colon alone.

`print` of a 4-digit number is what `updateCountdownDisplay` uses for `MMSS` (minutes * 100 + seconds). It then calls `drawColon` and `writeDisplay` so the colon can blink.

Segment bits for `writeDigitRaw` on a 7-segment digit: bit 0..6 are the usual A..G segments, bit 7 is the decimal point. The colon's raw mask at position 2 is `0x02` (both dots, hard-wired together).

## Other classes in the same library

Same `begin` / `clear` / `writeDisplay` / `setBrightness` / `blinkRate` pattern:

| Class | Device |
|---|---|
| `Adafruit_7segment` | 4-digit 7-segment. Timer countdown. |
| `Adafruit_AlphaNum4` | 4-digit 14-segment. Not used; strikes are raw. |
| `Adafruit_24bargraph` | 24-bar bi-color. |
| `Adafruit_8x8matrix` | 8x8 single color. Inherits GFX `drawPixel`. |
| `Adafruit_BicolorMatrix` | 8x8 bi-color. `drawPixel(x, y, LED_RED / LED_GREEN / LED_YELLOW / LED_OFF)`. |
| `Adafruit_16x8matrix` | 16x8 matrix. |

Matrices are GFX subclasses, so `fillRect`, `setCursor`, and `print` work after `begin`, still followed by `writeDisplay()`.

## Brightness and blink

```cpp
countdown.setBrightness(0);     // 0..15
countdown.setDisplayState(false);
countdown.blinkRate(HT16K33_BLINK_OFF);   // or _2HZ, _1HZ, _HALFHZ
```

Hardware blink is the whole display. The timer blinks the colon in software instead, faster once strikes are 2 or more.

## Two devices on one bus

`0x70` and `0x74` already share Wire. A third backpack needs a free address in `0x70`–`0x77` and its own object with that address. Call `begin` after `Wire.begin()`, once.
