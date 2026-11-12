# KTANE Module Build System

Scripts to build module firmware and copy UF2 files to `firmware/`.

## Usage

```powershell
.\build_all.ps1          # bomb firmware (modules, edgework, support except debugger)
.\build_debug.ps1        # CAN harnesses under support/debugger/
```

```bash
./build_all.sh
./build_debug.sh
./build_module.sh timer  # one module by leaf name or path
```

## Detection

`build_all` scans:

- `modules/*`
- `edgework/*`
- `support/*` except `support/debugger/`

Each child directory with a `platformio.ini` is built. Output still lands in `build/<name>/` per each project's `build_dir`.

`build_debug` builds every sketch under `support/debugger/`.

## Prerequisites

- PlatformIO CLI on PATH
- PowerShell (Windows) or bash (macOS/Linux)

## Manual build

```bash
cd modules/timer
pio run
```

## Layout

```
KTANE/
├── modules/timer/
├── modules/simon_says/
├── edgework/serial_number/
├── support/audio/
├── support/audio_test/
├── support/debugger/can_debug_send/
├── build/              # PlatformIO output
└── firmware/           # copied .uf2 files
```
