# ktane_console

Header macros that pick the default `Stream` for logs and helpers that take `Stream& output = KTANE_CONSOLE_OUT`.

## Usage

```cpp
#include <ktane_console.h>

void setup() {
  ktaneConsoleInit(115200);
  KTANE_CONSOLE_OUT.println(F("[MyModule] Ready"));
}
```

## `KTANE_PICOPROBE_UART`

When the build defines `-DKTANE_PICOPROBE_UART`, `KTANE_CONSOLE_OUT` is `Serial1` on GP0 (TX) / GP1 (RX) — Pico Probe UART — instead of USB CDC `Serial`.

Call `ktaneConsoleInit(baud)` once from `setup()` so the correct port is started.

Add `lib_extra_dirs = ../shared_libs` (or the repo-relative equivalent) in `platformio.ini` and include `<ktane_console.h>` where needed.
