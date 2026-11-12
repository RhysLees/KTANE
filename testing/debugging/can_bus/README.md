# CAN bus harnesses

Sketches under `testing/debugging/can_bus/` exercise `shared_libs/can_bus` without the full timer.

| Sketch | Role |
|--------|------|
| `can_test_master` | Timer stand-in. Default build **blocks instance 1** with synthetic `ID_TAKEN`. |
| `can_test_master` env `can_test_master_clear` | Observer only — expect slave instance **1**. |
| `can_test_slave` | Negotiates `CAN_TYPE_DEBUGGER_SENDER`, then runs traffic / latency tests. |
| `can_test_occupier` | Fixed **instance 1** holder (real `ID_TAKEN` replies via `can_bus`). |

Wiring: SPI0 CS GP17, INT GP20, 8 MHz, 500 kbit/s, 115200. Terminate both ends of a two-node bus.

**Console:** USB CDC (`Serial`) on both boards — no Picoprobe. Flash with `pio run -t upload` (picotool). Plug each Pico via USB; two `pio device monitor` sessions on the two COM ports (unplug one Pico to see which COM disappears if unsure).

## ID negotiation test matrix

| Run | Pico A | Pico B | Build / notes | Pass |
|-----|--------|--------|---------------|------|
| **A — conflict (default)** | `can_test_master` | `can_test_slave` | Master injects `ID_TAKEN` on every probe for instance 1 | Instance **2**, `ID_TAKEN conflict path`, then traffic tests |
| **B — clean** | `can_test_master_clear` | `can_test_slave` | `pio run -e can_test_master_clear` on master | Instance **1**, no synthetic block |
| **C — real occupier** | `can_test_master_clear` | `can_test_occupier` + **second bus node** with `can_test_slave` | Needs **3× Pico** (or run occupier and slave one at a time with a buddy board) | Slave instance **2**; occupier console shows `PROBE` / library `ID_TAKEN` |

Default master summary includes:

- `[PASS] Negotiated instance` — matches `CAN_TEST_EXPECT_INSTANCE`
- `[PASS] ID_TAKEN conflict path` — master sent synthetic `ID_TAKEN` (run A only)

Run **A** on every `can_bus` change; run **B** after negotiation logic edits; run **C** before relying on two same-type modules at once.

## Full automated sequence (master + slave)

1. Negotiation + registration (with checks above)
2. RX telemetry flood (non-blocking `handleCanMessages` gap stats)
3. TX heartbeat burst
4. Ping RTT
5. Slave stats (6-byte CAN payload)

## Build

```powershell
./build_test.ps1
```

Or: `pio run` / `pio run -e can_test_master_clear` in each sketch folder.

## Flash both boards (no drag-and-drop)

**Ctrl+C** any serial monitor on the Pico COM ports first (the port is exclusive).

```powershell
cd testing/debugging/can_bus
./flash_can_test_pair.ps1
```

Builds master + slave, resets each Pico over USB with the **1200 baud** bootloader trick, copies UF2 to **RPI-RP2**. Board order is set in `can_test_flash_map.ps1` (serials); override with `-MasterPort COM4` / `-SlavePort COM6`. `-SkipBuild` if UF2s are already built.

## Flash on Windows (picotool fails)

If `pio run -t upload` says **picotool was unable to connect** / use **Zadig**:

1. **UF2 (easiest):** One Pico only, hold **BOOTSEL**, plug USB → copy `build/<sketch>/<env>/firmware.uf2` onto the **RPI-RP2** drive. Or:

   ```powershell
   cd testing/debugging/can_bus
   ./flash_uf2.ps1 can_test_slave
   ```

2. **Zadig (once):** With Pico in BOOTSEL, [Zadig](https://zadig.akeo.ie/) → options → list all devices → **RP2 Boot** / Interface 0 → **WinUSB** → Replace. Then `pio run -t upload` works.

3. **One board at a time** — two Picos in BOOTSEL confuses picotool.

4. Unplug probe; use **USB data** port on each Pico (not power-only cables).

## Library hook

Harnesses may call `sendIdNegotiationFrame()` in `can_bus.h` to emit raw `ID_PROBE` / `ID_TAKEN` on a type’s instance-0 address.
