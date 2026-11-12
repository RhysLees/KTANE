# Timer setup

Pico W, Picoprobe on GP0/GP1, CAN on SPI0, two HT16K33 backpacks on I2C0. [Bus wiring](../../getting-started/bus-wiring.md) covers the MCP2515 pins shared with every other module. [Toolchain](../../getting-started/toolchain.md) covers `pio run`.

## Wiring

```text
GP0  UART0 TX  -> Picoprobe RX
GP1  UART0 RX  -> Picoprobe TX
GP8  I2C0 SDA  -> both backpacks SDA
GP9  I2C0 SCL  -> both backpacks SCL
GP16..GP20     -> MCP2515, see bus wiring

Countdown backpack A0..A2 open  -> I2C 0x70
Strike backpack    address strap -> I2C 0x74
```

Both backpacks share one I2C bus. The countdown sketch uses `Adafruit_7segment`. The strike sketch bit-bangs the HT16K33 command set and does not use the Adafruit alpha class. Pull-ups belong on SDA and SCL once, not once per backpack, if the backpacks already have them.

Do not move I2C back to GP4/GP5 unless you mean to. Do not use GP0 or GP1 for the displays.

`setup()` waits about 2 seconds after the console starts, then calls `randomSeed(rp2040.hwrand32())` before `Wire.begin()`.

## Flash

```powershell
cd modules/timer
pio run --target upload
pio device monitor
```

`board` is `rpipicow`. The first boot with empty EEPROM starts an access point. Credentials are stored with `EEPROM.begin` / `put` / `commit` (512 bytes, magic `"WIFI"` at address 0). `EEPROM.commit` stalls the other core. This sketch has no `loop1`, so that stall does not drop CAN, but do not move the web server onto core 1.

## Join the setup page

1. Power the timer. With no stored network, the serial log prints `Starting WiFi AP: KTANE_TIMER_SETUP` and the soft-AP address (the Pico W default is `192.168.4.1`).
2. Join WiFi `KTANE_TIMER_SETUP`. Password `ktane12345`.
3. Open `http://192.168.4.1/`. A DNS listener on port 53 answers lookups with the AP address so a captive-portal probe lands on the same page.
4. Paths other than `/`, `/wifi`, and `/api/...` redirect to the AP while it is in setup mode.
5. To leave setup mode, POST `/api/wifi` with an SSID and password. The timer saves them and tries to join. If the join fails, the log says the credentials were saved and it will try again on restart.

The HTTP server is `WiFiServer` on port 80. JSON bodies are ArduinoJson 6. Keep new documents short-lived and on the heap (`DynamicJsonDocument`).

## Confirm the bus

1. Flash at least one other module (Simon, serial tag, or audio) and power it on the same CAN pair, with 120 ohm termination only at the two ends.
2. Open the timer page. Gameplay modules (Simon and anything cloned from it) appear in `GET /api/modules`. Audio and the serial tag are tracked apart from that list. `GET /api/all` returns them as `status.audioModules` and `status.serialModules`, which is what the page renders. `GET /api/audio` reports whether the audio module is connected.
3. A healthy module heartbeats every 3 seconds before the game and every 1 second during it. The timer marks it missing after 6 seconds of silence in discovery, or 2 seconds during a game.
4. Start a round from the page (`POST /api/command` with `{"command":"start"}`, or the button that sends that). The countdown leaves `----` and the colon starts blinking.

## Console

Logs go to `Serial1` at 115200, not USB CDC, because `-DKTANE_PICOPROBE_UART` is set. `pio device monitor` must be pointed at the probe's port. The checked-in `monitor_port` is a macOS `cu.usbmodem` path. On Windows, point it at the probe COM port.

The root README mentions UART commands (`CAN`, `STATUS`, `START`, `RESET`). Those belong to the debug menu in `timer/lib/debug/`, which `main.cpp` does not start. The live control surface is the web API. See [Web API](web-api.md).
