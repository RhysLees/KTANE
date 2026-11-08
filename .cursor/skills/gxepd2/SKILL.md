---
name: gxepd2
description: >-
  Drive the 2.66 inch 3-color e-paper serial tag with GxEPD2 on SPI1. Use
  when editing serial_number/lib/epaper, changing the tag layout, or calling
  GxEPD2_266c, firstPage, nextPage, or hibernate.
---

# GxEPD2

`zinggjm/GxEPD2@^1.6.4`. The serial module includes `<GxEPD2_3C.h>` (black / white / red). `<GxEPD2_BW.h>` is the black-white-only header. This panel is 3-color, so stay on `GxEPD2_3C`.

Class in `serial_number/lib/epaper/epaper.cpp`:

```cpp
GxEPD2_3C<GxEPD2_266c, GxEPD2_266c::HEIGHT> epaper(
    GxEPD2_266c(/*CS*/ 5, /*DC*/ 6, /*RST*/ 7, /*BUSY*/ 8));
```

`GxEPD2_266c` is the 2.66" 152x296 3-color controller (GDEY0266Z90 class). The second template argument is the page height in pixels. `HEIGHT` is the full panel, so there is a single page and the controller buffer is large. If a future panel makes that allocation fail, drop the page height (a divisor of the panel height) and keep the `firstPage` / `nextPage` loop; the library replays drawing once per page.

## Pins and SPI1

CAN owns SPI0. The panel is on SPI1.

| Signal | GPIO |
|---|---|
| CS | GP5 |
| DC | GP6 |
| RST | GP7 |
| BUSY | GP8 |
| SCK | GP10 |
| MOSI | GP11 |

MISO is unused. Set the SPI1 pins before `begin()`, then hand that object to `init`:

```cpp
SPI1.setSCK(10);
SPI1.setTX(11);
SPI1.begin();

SPISettings settings(115200, MSBFIRST, SPI_MODE0);
epaper.init(/*diag baud*/ 115200,
            /*initial*/ true,
            /*reset pulse ms*/ 2,
            /*pulldown reset*/ false,
            SPI1, settings);
epaper.setRotation(1);     // landscape
epaper.setFullWindow();
```

`init` arguments, in order: diagnostic serial baud (0 to silence), whether to fill the controller RAM, reset pulse length, pulldown-reset for some ESP boards (false on Pico), the `SPIClass`, and `SPISettings`. The 6-argument form is what lets this board leave SPI0 alone. The 4-argument `init(baud, initial, reset, pulldown)` uses the global `SPI` and will fight the CAN controller.

BUSY is an input. Do not `pinMode` it as an output.

## Drawing

3-color updates are full refreshes and take several seconds. `serial_number` runs them on core 1 (`setup1` / `loop1`) and queues text from core 0 so CAN stays serviced. Follow that split for any new screen.

Paged loop, copied from the driver:

```cpp
epaper.setFullWindow();
epaper.firstPage();
do {
  epaper.fillScreen(GxEPD_WHITE);
  epaper.fillRect(0, 0, epaper.width(), epaper.height() / 2, GxEPD_RED);
  epaper.setFont(&resolution_medium24pt7b);
  epaper.setTextColor(GxEPD_WHITE);
  epaper.setCursor(x, y);
  epaper.print("SERIAL #");
} while (epaper.nextPage());
epaper.hibernate();
```

`firstPage` fills the current page white and resets the page counter. The `do` body runs once per page; `nextPage` ships that page and returns true while pages remain. After the last page it refreshes the glass and powers the panel down.

Colors: `GxEPD_WHITE`, `GxEPD_BLACK`, `GxEPD_RED`. Drawing is GFX. See [adafruit-gfx](../adafruit-gfx/SKILL.md). Text outside the `do` block is thrown away on the next page clear.

`epaper.display(false)` is a full refresh if you drew with `setFullWindow` and GFX without the page loop. This module uses the page loop, then `display()` is redundant with the refresh inside `nextPage`. `hibernate()` after a finished frame drops the panel supply. The next `init` / draw cycle wakes it. `powerOff()` is the lighter sleep; `hibernate()` is what the tag uses between serial numbers.

## Partial windows

```cpp
epaper.setPartialWindow(x, y, w, h);   // w multiple of 8
epaper.firstPage();
do { /* draw the whole window contents */ } while (epaper.nextPage());
```

`GxEPD2_266c` is a 3-color panel. `hasFastPartialUpdate` is false on these controllers. A "partial" window still runs a slow color refresh and flashes. Keep `setFullWindow` for the serial tag.

`setRotation` values are 0, 1, 2, 3. Width and height swap for 90 and 270 degrees. Call it once after `init`, before measuring text.

## Other calls that are safe here

- `epaper.clearScreen()` fills controller RAM and refreshes white. Slow. Fine at boot.
- `epaper.writeImage(...)` pushes a 1-bit bitmap. Black and red are separate buffers on 3-color types.
- `epaper.epd2.panel` identifies the controller if one binary must support two panels. This module is compiled for one.

Construct the display once. `init` once. Draw only from the core that called `init`. GxEPD2 is not safe to share across cores.
