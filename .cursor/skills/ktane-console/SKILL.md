---
name: ktane-console
description: >-
  Log KTANE firmware through ktane_console (KTANE_CONSOLE_OUT). Use when
  printing from a module or shared library, or when Picoprobe UART and USB
  CDC both seem to be the console.
---

# ktane_console

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
| `platformio.ini` has `-DKTANE_PICOPROBE_UART` | `Serial1` (UART0) | TX GP0, RX GP1 |
| flag absent | `Serial` (USB CDC) | USB |

Every current module passes the flag. A library that prints to `Serial` directly is invisible on the Picoprobe monitor. Take a `Stream&` argument defaulting to `KTANE_CONSOLE_OUT`, which is what `printSdCardInfo` does.

`Serial1.setTX(0)` and `setRX(1)` run inside `ktaneConsoleInit`. Do not call `Serial1.begin` again, and do not put I2C or a button on GP0 or GP1.

`F()` and `print` / `println` / `printf` are ordinary Arduino `Print` calls. Core 1 may print too; lines can interleave. Keep core-1 logs short.

`monitor_speed = 115200` and `monitor_flags = --echo` match this init. `--echo` shows what you type into the timer's serial command parser.
