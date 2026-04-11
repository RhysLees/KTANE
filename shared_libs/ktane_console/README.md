# ktane_console

Header macros that pick the default `Stream` for logs and helpers that take `Stream& output = KTANE_CONSOLE_OUT`.

## Usage

```cpp
#include <ktane_console.h>

void setup() {
  Serial.begin(115200);
  KTANE_CONSOLE_OUT.println(F("[MyModule] Ready"));
}
```

## `EDGE_AUDIO_CONSOLE`

When the build defines `-DEDGE_AUDIO_CONSOLE`, `KTANE_CONSOLE_OUT` is `Serial1` instead of `Serial` (e.g. Picoprobe UART on GP0/GP1 while USB CDC stays free).

Add `lib_extra_dirs = ../shared_libs` (or the repo-relative equivalent) in `platformio.ini` and include `<ktane_console.h>` where needed.
