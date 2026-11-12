# Console

`shared_libs/ktane_console/`. Include `<ktane_console.h>`.

```cpp
void setup() {
  ktaneConsoleInit(115200);
  KTANE_CONSOLE_OUT.println(F("[Wires] ready"));
}
```

`ktaneConsoleInit(unsigned long baud = 115200)` starts the port. Call it once, before any log, from `setup` on core 0.

| Build | `KTANE_CONSOLE_OUT` | Pins |
|---|---|---|
| `-DKTANE_PICOPROBE_UART` in `platformio.ini` | `Serial1` (UART0) | TX GP0, RX GP1 |
| flag absent | `Serial` (USB CDC) | USB |

Every current module passes the flag, including `audio_test`. A library that prints to `Serial` directly is invisible on the Picoprobe monitor. The header comment that says most modules use USB CDC describes the flag-absent default, not the sketches as they are built.

Take a `Stream&` argument defaulting to `KTANE_CONSOLE_OUT` when a helper prints. `printSdCardInfo` does that.

`Serial1.setTX(0)` and `setRX(1)` run inside `ktaneConsoleInit`. Do not call `Serial1.begin` again, and do not put I2C or a button on GP0 or GP1.

`F()`, `print`, `println`, and `printf` are ordinary Arduino `Print` calls. Core 1 may print too. Lines can interleave. Keep core-1 logs short.

`monitor_speed = 115200` and `monitor_flags = --echo` match this init. `--echo` shows what you type into a serial parser. The timer's live UI is the web page. The debug menu that would read those keystrokes is not started from `timer/src/main.cpp`.

The checked-in `monitor_port` values are macOS `cu.usbmodem` paths. On Windows, point `pio device monitor` at the probe's COM port. USB CDC on a Pico W shows up as a second port and will not print these logs while the flag is set.
