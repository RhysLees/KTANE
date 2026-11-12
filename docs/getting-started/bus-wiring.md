# Bus wiring

Every bomb module carries the same MCP2515 on SPI0. Puzzle pins change. These do not.

## CAN controller, every board

| Signal | GPIO | Notes |
|---|---|---|
| CS | GP17 | `can_bus` calls `SPI.begin()` itself |
| INT | GP20 | active low, falling edge |
| MISO | GP16 | SPI0 default |
| SCK | GP18 | SPI0 default |
| MOSI | GP19 | SPI0 default |
| Crystal | 8 MHz | `MCP_8MHZ` in `initCanBus` |
| Bit rate | 500 kbit/s | `CAN_500KBPS` |

```text
Pico GP16 MISO ---- MCP2515 SO
Pico GP17 CS   ---- MCP2515 CS
Pico GP18 SCK  ---- MCP2515 SCK
Pico GP19 MOSI ---- MCP2515 SI
Pico GP20      ---- MCP2515 INT

MCP2515 CANH/CANL ---- twisted pair ---- next module
```

Daisy-chain CANH to CANH and CANL to CANL. Share a ground between every Pico and every transceiver. Fit the on-board 120 ohm jumper (J1) on the two nodes at the physical ends of the pair only. A third jumper, a missing pair of jumpers, or a 16 MHz crystal constant shows up as bus-off: `begin` can succeed and then no frame is ACKed.

`enOneShotTX` is on, so a module powered up alone does not retry a frame forever while nobody ACKs. `handleCanMessages` retries a failed one-shot send up to three times.

SPI0 is taken. SD cards and the e-paper panel use SPI1. Do not call `SPI.begin()` again from module code after `initCanBus`, and do not point a display at the default `SPI` object.

## Console, every board

| Build flag | Port | Pins |
|---|---|---|
| `-DKTANE_PICOPROBE_UART` (every current sketch) | `Serial1`, UART0 | TX GP0, RX GP1 |
| flag absent | USB `Serial` | USB |

115200 baud. `ktaneConsoleInit` calls `Serial1.setTX(0)` and `setRX(1)`. Do not call `Serial1.begin` again, and do not put I2C or a button on GP0 or GP1. A library that prints to `Serial` directly is invisible on the probe. Core 1 may print too. Lines can interleave. Keep those logs short.

## Power-up order

Modules can boot in any order. A gameplay module probes for a free instance until `isCanIdReady()`, then registers once a second until the timer answers `TIMER_MODULE_DISCOVERED`. The timer can boot last. Heartbeats before the game run every 3 seconds, and the timer allows 6 seconds of silence during discovery.

Bring-up that isolates the controller from the protocol lives in `test_modules/` and is outside this guide. If a new board is silent, confirm the crystal is 8 MHz before changing protocol code.

## Per-module wiring

| Module | Extra buses | Guide |
|---|---|---|
| Timer | I2C0 on GP8/GP9, two HT16K33s | [Timer setup](../modules/timer/setup.md) |
| Simon Says | buttons and LEDs on GP2–GP9, status LED GP11 | [Simon setup](../modules/simon-says/setup.md) |
| Serial tag | SPI1 e-paper | [Serial setup](../modules/serial-number/setup.md) |
| Audio | I2S, I2C1 codec, SPI1 SD | [Audio setup](../modules/audio/setup.md) |
