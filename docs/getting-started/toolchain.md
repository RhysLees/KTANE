# Toolchain

Every sketch builds with PlatformIO on the Earle Philhower Arduino core, pulled from `https://github.com/maxgerhardt/platform-raspberrypi.git`. Upload and debug use CMSIS-DAP (a Picoprobe or a Pico flashed as one).

## Install

1. Install [PlatformIO](https://docs.platformio.org/) so `pio` is on `PATH`.
2. From a module directory, the first `pio run` downloads the platform and the libraries in that module's `platformio.ini`.
3. Connect the Picoprobe. Leave the target's GP0 (UART0 TX) and GP1 (UART0 RX) wired to the probe if you want the console. USB CDC is not the log port while `-DKTANE_PICOPROBE_UART` is set.

Checked-in `monitor_port` values are macOS paths (`/dev/cu.usbmodem11102`). On Windows, set `monitor_port` to the probe's COM port, or delete the line and let PlatformIO pick the CMSIS-DAP device. `upload_protocol = cmsis-dap` does not need a BOOTSEL button when the probe is attached.

## Build one module

```powershell
cd timer
pio run
pio run --target upload
pio device monitor
```

`monitor_speed` is 115200. `monitor_flags = --echo` shows what you type. That matters on the timer if you revive the serial command parser. The running timer UI is the WiFi page, not the UART.

Build output goes to `build/<module>/` because each `platformio.ini` sets `build_dir`. Do not build from the repository root. There is no root `platformio.ini`.

## Build every module

From the repository root:

```powershell
.\build_all.ps1
```

The script:

1. Scans root directories for `platformio.ini`.
2. Skips `DOCS`, `shared_libs`, `build`, `firmware`, `.git`, `.vscode`, `.pio`, and names matching `can_debug*` or `can_test*`.
3. Runs `pio run` in each remaining directory. That includes `audio_test` as well as the bomb modules. `docs/` has no `platformio.ini`, so it is ignored. `test_modules/` is nested, so those four sketches are ignored.
4. Copies each `.uf2` into `firmware/` as `<module>_<uf2 name>`.

A failed module increments the failure count and the script continues with the next directory.

## Boards

| Sketch | `board` | Why |
|---|---|---|
| `timer`, `audio`, `audio_test` | `rpipicow` | WiFi on the timer. Bluetooth headers are compiled into audio. |
| `simon_says`, `serial_number` | `rpipico` | No radio. |

Debug builds are the default: `build_type = debug` and `debug_build_flags = -O0 -ggdb3 -g3`. `debug_tool = cmsis-dap`.

## Libraries that must stay put

| Dependency | Where | Constraint |
|---|---|---|
| `coryjfowler/mcp_can@^1.5.1` | every CAN sketch | 8 MHz crystal, 500 kbit/s. `MCP_16MHZ` inits and then sees nothing on these boards. |
| `bblanchon/ArduinoJson@^6.21.0` | timer | Version 7 removes `DynamicJsonDocument`. |
| `adafruit/Adafruit LED Backpack Library` | timer | Countdown only. Strikes talk to the HT16K33 directly. |
| `zinggjm/GxEPD2@^1.6.4` | serial tag | 3-color 2.66 inch panel on SPI1. |
| `pschatzmann/arduino-audio-tools` | audio, audio_test | I2S and WAV. |
| `adafruit/Adafruit_TLV320_I2S` | audio, audio_test | Headphone codec on `Wire1`. |

Shared code is compiled through `lib_extra_dirs = ../shared_libs`. Those folders have no `library.properties`. `audio_test` also lists `../audio/lib` so it can reuse the SD pin header and the sound-bank paths.

## Flash a UF2 without the probe

Hold BOOTSEL, plug in USB, and copy `firmware/<module>_*.uf2` onto the `RPI-RP2` drive. The console still expects a Picoprobe on GP0/GP1 unless you remove `-DKTANE_PICOPROBE_UART` and log to USB `Serial`.
