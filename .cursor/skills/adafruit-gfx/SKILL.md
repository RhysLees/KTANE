---
name: adafruit-gfx
description: >-
  Draw with Adafruit GFX on the KTANE e-paper and any future GFX display.
  Use when editing serial_number fonts or layout, adding a TFT or OLED, or
  calling setFont, getTextBounds, fillRect, print, or GFXfont.
---

# Adafruit GFX

`adafruit/Adafruit GFX Library`. The timer lists it in `lib_deps` because the LED backpack includes it. The serial display gets it through GxEPD2. Include `<Adafruit_GFX.h>`.

GFX is a base class. It does not talk to hardware. The display subclass (`GxEPD2_3C<...>`, `Adafruit_SSD1306`, `Adafruit_ILI9341`, ...) implements `drawPixel`. Everything else is built on that.

Construct the concrete display, then draw. Calling GFX methods on the e-paper only updates the panel inside a `firstPage` / `nextPage` loop. See [gxepd2](../gxepd2/SKILL.md). The 7-segment countdown does not use these draw calls; it uses `writeDigitNum`. See [adafruit-led-backpack](../adafruit-led-backpack/SKILL.md).

## Color

`uint16_t` colors are RGB565 on color TFTs (`0xF800` red, `0x07E0` green, `0x001F` blue). Helpers: `display.color565(r, g, b)`.

On the 3-color e-paper, pass the panel constants, not a made-up RGB565:

- `GxEPD_WHITE` background
- `GxEPD_BLACK` black ink
- `GxEPD_RED` red ink

Red and black are separate planes. A "gray" RGB565 value will not dither.

## Text

Built-in 6x8 font, scaled by `setTextSize(n)`:

```cpp
display.setTextWrap(true);
display.setTextSize(2);
display.setTextColor(GxEPD_BLACK);
display.setCursor(0, 0);          // top-left of the next print, in pixels
display.print("SERIAL");
display.println(serial);
```

`setTextColor(color, background)` fills the cell behind each glyph. Use it when you overwrite text without clearing the screen.

Custom fonts are `GFXfont` blobs from Adafruit's `fontconvert`. This repo already has `resolution_medium24pt7b` and `resolution_medium36pt7b` in `serial_number/lib/fonts/`.

```cpp
#include <fonts.h>
display.setFont(&resolution_medium36pt7b);
display.setTextSize(1);            // size > 1 looks wrong on these fonts
```

With a custom font, `setCursor(x, y)` is the baseline, not the top. Center with `getTextBounds`:

```cpp
int16_t x1, y1;
uint16_t w, h;
display.getTextBounds(text, 0, 0, &x1, &y1, &w, &h);
int16_t x = (display.width() - (int16_t)w) / 2 - x1;
int16_t y = boxY + (boxH - (int16_t)h) / 2 + (int16_t)h;
display.setCursor(x, y);
display.print(text);
```

`setFont()` or `setFont(nullptr)` returns to the built-in font. `cp437(true)` selects the classic code page if you need box-drawing glyphs from the built-in font.

## Shapes

All coordinates are pixels, origin top-left after `setRotation`.

```cpp
display.drawPixel(x, y, color);
display.drawLine(x0, y0, x1, y1, color);
display.drawFastVLine(x, y, h, color);
display.drawFastHLine(x, y, w, color);
display.drawRect(x, y, w, h, color);
display.fillRect(x, y, w, h, color);
display.drawCircle(x, y, r, color);
display.fillCircle(x, y, r, color);
display.drawRoundRect(x, y, w, h, radius, color);
display.fillRoundRect(x, y, w, h, radius, color);
display.drawTriangle(x0, y0, x1, y1, x2, y2, color);
display.fillTriangle(x0, y0, x1, y1, x2, y2, color);
display.fillScreen(color);
display.drawBitmap(x, y, bitmap, w, h, color);          // 1-bit, PROGMEM
display.drawBitmap(x, y, bitmap, w, h, color, bg);
```

`getTextBounds`, `width()`, `height()`, `getRotation`, and `setRotation(0..3)` are on the GFX object. Rotation 1 is landscape on the 2.66" panel (`serial_number/lib/epaper/epaper.cpp`).

`invertDisplay(bool)` depends on the subclass. The e-paper driver ignores it.

## Canvas, when you must compose off-screen

```cpp
#include <Adafruit_GFX.h>
GFXcanvas1 mono(128, 64);     // 1-bit
GFXcanvas16 color(128, 64);   // RGB565
mono.fillScreen(0);
mono.setCursor(0, 8);
mono.print("HI");
display.drawBitmap(0, 0, mono.getBuffer(), 128, 64, GxEPD_BLACK, GxEPD_WHITE);
```

`GFXcanvas1` is the right size for a small OLED. A full-screen RGB565 canvas of the 2.66" panel does not fit in 264 KB. The e-paper driver already pages its own buffer; draw straight into the `firstPage` loop.

## Adding another GFX display

1. Depend on the device library (it will pull in GFX and `Adafruit BusIO`).
2. On this core, call `Wire.setSDA` / `SPI1.setSCK` before `begin()`. SPI0 belongs to CAN.
3. Draw, then call the device's flush (`display.display()` for SSD1306, `nextPage` for GxEPD2). GFX itself never flushes.
