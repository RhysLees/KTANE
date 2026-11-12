# Serial number setup

Pico, Picoprobe on GP0/GP1, CAN on SPI0, a 2.66 inch 3-color e-paper tag on SPI1. Shared controller pins are in [Bus wiring](../../getting-started/bus-wiring.md).

## Wiring

```text
GP0  UART0 TX -> Picoprobe RX
GP1  UART0 RX -> Picoprobe TX

GP5  panel CS
GP6  panel DC
GP7  panel RST
GP8  panel BUSY
GP10 panel SCK   SPI1
GP11 panel MOSI  SPI1

GP16..GP20 MCP2515 on SPI0
```

The panel needs no MISO. SPI clock in `epaper.init` is 115200. That is the panel's SPI rate, not the console baud.

`setup()` also starts `Wire` on GP8/GP9. The panel's BUSY pin is GP8. Leave GP8 for BUSY. Do not hang a real I2C device on that pair unless you move one of them in the source first.

Power the panel from the rail its board expects, and share ground with the Pico. A missing ground looks like a BUSY pin that never releases, and core 1 sits in the refresh while core 0 keeps heartbeating.

## Flash

```powershell
cd edgework/serial_number
pio run --target upload
pio device monitor
```

`board` is `rpipico`. Libraries: `mcp_can` and `zinggjm/GxEPD2@^1.6.4`. Adafruit GFX arrives with GxEPD2. Fonts are compiled from `lib/fonts/`.

There is no status LED. The module still registers and heartbeats, so the timer can show it connected.

## Bring-up

1. Power the timer and this board on the same terminated CAN pair.
2. Wait through the 5 second startup spin. Core 0 is already calling `handleCanMessages` during that wait. The module has no status LED. It still registers at `0x400`. The setup page shows it under `status.serialModules` from `GET /api/all`, not in `GET /api/modules`. The first full-panel refresh happens when a serial arrives, not at boot, unless `module_state` already has one.
3. Start a game, or reset the timer so it broadcasts the serial halves. The tag should go red on the top half, with `SERIAL #`, and black on the bottom half with six characters.
4. The refresh takes several seconds. Heartbeats continue on core 0 while it runs. If you also queue clear or credit during a refresh, they wait in the 4-deep queue.

`SERIAL_DISPLAY_SHOW_CREDIT` draws `KTANE IRL` / `By Rhys Lees`. `SERIAL_DISPLAY_CLEAR` fills the panel white. Both are one-byte frames to `CAN_ID_SERIAL_DISPLAY` (`0x400`). The timer's live path does not send them. The serial you see in a real round comes from `TIMER_SERIAL_NUMBER_FIRST_HALF` and `TIMER_SERIAL_NUMBER_LAST_HALF`, reassembled by `module_state`.

If `status.serialModules` shows the board connected and the glass stays blank, the queue is not the problem yet: confirm a serial callback fired in the probe log, then confirm BUSY, RST, and SPI1 (GP10/GP11, not the CAN pins on GP18/GP19).
