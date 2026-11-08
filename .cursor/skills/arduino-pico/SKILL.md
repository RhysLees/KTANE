---
name: arduino-pico
description: >-
  How to write firmware on the Earle Philhower Arduino-Pico core used by every
  KTANE module (PlatformIO board_build.core = earlephilhower). Use when adding
  GPIO, UART, I2C, SPI, WiFi, EEPROM, Bluetooth, a second core, PWM, or any
  Arduino API, or when choosing between Pico SDK, mbed, and Arduino.
---

# Arduino-Pico (Earle Philhower)

Every module compiles as an Arduino sketch against this core, via

```ini
platform = https://github.com/maxgerhardt/platform-raspberrypi.git
framework = arduino
board_build.core = earlephilhower
board = rpipico        ; RP2040, no radio
; board = rpipicow     ; CYW43439 — timer and audio only
```

`board = rpipico` / `rpipicow` already select this core. Keep `board_build.core = earlephilhower` anyway so a copied `board = pico` stanza cannot fall back to ArduinoCore-mbed. mbed has different default pins, no `Wire.setSDA`, and a different multicore model.

The core is a thin Arduino wrapper over the Pico SDK. Application code stays Arduino: `setup` / `loop`, `pinMode`, `digitalRead`, `Wire`, `SPI`, `Serial`. SDK headers are available when Arduino has no equivalent. This repo uses that once: `pico/mutex.h` in `audio/lib/sd_card/sd_card.cpp`. Do not add a `CMakeLists.txt`, call `stdio_init_all`, or start `multicore_launch_core1` yourself.

Docs: <https://arduino-pico.readthedocs.io/en/latest/>

## Pins

GPIO numbers are the Arduino pin numbers. GP17 is `17`.

Most peripherals can move, but only onto pins the RP2040 mux allows for that block. Call the setter before `begin()`. `setSDA` / `setSCL` return false when the pin is not on that I2C block.

| Object | Hardware | Setters, before `begin()` |
|---|---|---|
| `Serial` | USB CDC | `Serial.begin(baud)` |
| `Serial1` | UART0 | `setTX`, `setRX`. This repo: GP0 / GP1 via `ktaneConsoleInit` |
| `Serial2` | UART1 | `setTX`, `setRX` |
| `Wire` | I2C0 | `setSDA`, `setSCL` |
| `Wire1` | I2C1 | `setSDA`, `setSCL` |
| `SPI` | SPI0 | `setRX` / `setMISO`, `setTX` / `setMOSI`, `setSCK`, `setCS` |
| `SPI1` | SPI1 | same |

Pico SPI0 defaults are MISO GP16, CS GP17, SCK GP18, MOSI GP19. `can_bus` calls `SPI.begin()` with those defaults and chip-select GP17. Modules that call `initCanBus` must leave SPI0 on those pins. SD and e-paper go on SPI1.

`SPI.begin()` defaults to software chip-select: the device library toggles CS. `SPI.begin(true)` enables hardware CS on the `setCS` pin.

GP0 and GP1 are the Picoprobe UART (`-DKTANE_PICOPROBE_UART`) and the SWD pads. Do not attach I2C or buttons there. Timer and serial display put I2C0 on GP8 / GP9 for that reason.

`analogWrite(pin, 0..255)` is hardware PWM. `analogRead` is ADC0–2 (GP26–GP28) plus the internal temperature sensor. ADC pins are not used today.

`pinMode`, `digitalWrite`, `digitalRead`, `INPUT_PULLUP`, `attachInterrupt(..., FALLING)`, and `millis` behave as on other Arduino cores. `rp2040.hwrand32()` is the hardware RNG. Seed with it (`timer/src/main.cpp`). `F("...")` and `__FlashStringHelper` work; the audio logs use them to keep strings out of RAM.

## USB, UART, and the console

`Serial` is the USB CDC port. It does not block waiting for a host. `ktane_console` picks the stream:

- default: `KTANE_CONSOLE_OUT` is `Serial`
- `-DKTANE_PICOPROBE_UART`: `Serial1` at GP0 TX / GP1 RX, 115200

Call `ktaneConsoleInit(115200)` once from `setup` before any log. Libraries take `Stream& output = KTANE_CONSOLE_OUT`. See [ktane-console](../ktane-console/SKILL.md).

`monitor_port` in `platformio.ini` is a per-desk Picoprobe path. Change it locally; do not commit a machine-specific port as if it were universal.

## Second core

```cpp
void setup() { /* core 0 */ }
void loop() { /* core 0 */ }
void setup1() { /* core 1, starts with setup() */ }
void loop1() { /* starts when setup1() returns, not when setup() returns */ }
```

Core 1 has no Arduino background work. USB and the CYW43 driver live on core 0. A flash erase or write from core 0 (EEPROM commit, filesystem) pauses core 1 until the write finishes.

The hardware inter-core FIFO is owned by the core (idle / resume). Talk between cores with atomics or a software queue, as `audio/lib/audio_mixer` does. `rp2040.fifo` is that software FIFO if a small queue is enough.

Who runs where in this repo:

| Module | Core 0 | Core 1 |
|---|---|---|
| `audio/` with `EDGE_AUDIO` | I2S mixer only | CAN, module state, SD, stream feeder |
| `serial_number/` | CAN, module state | e-paper refresh (multi-second) |
| `audio_test/` | codec + I2S setup | `AudioPlayer.copy()` |
| everything else | all of `loop` | empty `loop1` |

If CAN is serviced on core 1, a long SD read has to pump `handleCanMessages()` itself. `audio/src/main.cpp` does that through `ktaneAudioBlockingYield`.

## EEPROM

Pico EEPROM is a 4096-byte flash emulation. The timer stores WiFi credentials with it.

```cpp
#include <EEPROM.h>

EEPROM.begin(128);          // size <= 4096, call before get/put
EEPROM.get(addr, creds);    // POD struct
EEPROM.put(addr, creds);    // stages only
EEPROM.commit();            // the flash write; stalls core 1
EEPROM.end();
```

`put` without `commit` does not survive reset.

## WiFi (Pico W only)

`#include <WiFi.h>`. The API matches the ESP8266 Arduino WiFi library. Timer is the only module that uses it (`timer/lib/web_server/web_server.cpp`).

```cpp
WiFi.mode(WIFI_STA);
WiFi.begin(ssid, password);
while (WiFi.status() != WL_CONNECTED) { delay(500); }
Serial.println(WiFi.localIP());

WiFi.mode(WIFI_AP);
WiFi.softAP("KTANE_TIMER", "ktane12345");
WiFi.softAPIP();            // 192.168.42.1 in the current firmware

WiFiServer server(80);
server.begin();
WiFiClient client = server.available();
```

Modes: `WIFI_STA`, `WIFI_AP`, `WIFI_AP_STA`, `WIFI_OFF`. Status: `WL_CONNECTED`, `WL_DISCONNECTED`, `WL_IDLE_STATUS`, `WL_CONNECT_FAILED`, `WL_NO_SSID_AVAIL`.

The CYW43 needs regular core-0 time. Keep `updateWebServer()` on core 0, and keep the I2S mixer off that core when WiFi is busy. There is no `ESP8266WebServer` here; the timer parses HTTP from `WiFiClient` by hand.

## Bluetooth (Pico W only)

Enable the stack with `-DPIO_FRAMEWORK_ARDUINO_ENABLE_BLUETOOTH` (already set on `audio/`). Includes: `<BluetoothHCI.h>`, `<BluetoothAudio.h>`.

`audio/lib/bluetooth/` can scan with `BluetoothHCI` and stream with `A2DPSource`. The shipping audio path does not call it. The bomb sound path is I2S into the TLV320. Bluetooth and WiFi share the CYW43; bringing A2DP up on the audio module while it is mixing on a deadline needs a measured test, not a drive-by enable.

## Debug and upload

```ini
upload_protocol = cmsis-dap
debug_tool = cmsis-dap
monitor_speed = 115200
monitor_flags = --echo
build_type = debug
debug_build_flags = -O0 -ggdb3 -g3
```

Core debug logs, if you need them while bringing up a bus: `-DDEBUG_RP2040_PORT=Serial1` plus any of `-DDEBUG_RP2040_CORE`, `-DDEBUG_RP2040_SPI`, `-DDEBUG_RP2040_WIRE`. Leave those off in normal play; they are noisy at 115200.

UF2 drag-install still works (BOOTSEL). `build_all.ps1` copies the UF2 into `firmware/`.

## Practices that match this repo

- Configure a peripheral's pins, then `begin()` once. `can_bus` owns `SPI.begin()` for SPI0.
- Share a bus across cores only behind a lock. SdFat uses `mutex_t` from `pico/mutex.h`.
- Keep `loop` on the CAN core short. `delay` there drops button edges and heartbeats.
- Prefer `Wire` / `SPI` transactions over poking `i2c0_hw`. The TLV320 driver is the exception: one raw page write the Adafruit helper does not cover (`audio/lib/amp/amp.cpp`).
- Check `setSDA` / `setSCL` / `setRX` results when moving a pin. A silent false means the pad is on the other I2C or SPI block.
