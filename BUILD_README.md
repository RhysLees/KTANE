# KTANE Module Build System

This directory contains scripts to build all KTANE module firmwares at once and automatically copy the resulting UF2 files to a central firmware directory.

## Usage

```powershell
.\build_all.ps1
```

## What the Scripts Do

1. **Auto-Detect Modules**: Automatically discovers all module directories by:
   - Scanning all directories in the project root
   - Excluding non-module directories (DOCS, shared_libs, build, firmware, .git, .vscode, .pio)
   - Checking for `platformio.ini` files to confirm they're valid modules
   - Currently detects: `audio`, `serial_number`, `simon_says`, `timer`

2. **Copy UF2 Files**: After successful builds, copies all `.uf2` files from the build directories to a central `firmware/` directory with descriptive names:
   - `audio_firmware.uf2`
   - `serial_number_firmware.uf2`
   - `simon_says_firmware.uf2`
   - `timer_firmware.uf2`

3. **Build Summary**: Shows success/failure status for each module and lists available firmware files.

## Prerequisites

- PlatformIO CLI installed and available in PATH
- PowerShell (available by default on Windows 10/11)

## Adding New Modules

The build scripts automatically detect new modules! Simply:

1. Create a new directory for your module
2. Add a `platformio.ini` file to the directory
3. Run the build script - it will automatically detect and build your new module

## Customizing Module Detection

If you need to exclude additional directories from module detection, edit the exclusion list in the scripts:

Edit the exclusion list in `build_all.ps1`:
```powershell
$excludeDirs = @("DOCS", "shared_libs", "build", "firmware", ".git", ".vscode", ".pio")
```

## Manual Building (Original Method)

If you prefer to build modules individually:

```bash
# Audio module
cd audio
pio run

# Serial number module
cd ../serial_number
pio run

# Simon Says module
cd ../simon_says
pio run

# Timer module
cd ../timer
pio run
```

## Directory Structure

```
KTANE/
├── audio/              # Audio module source
├── serial_number/      # Serial number module source  
├── simon_says/         # Simon Says module source
├── timer/              # Timer module source
├── build/              # Build output directory
│   ├── audio/          # Audio build files
│   ├── serial_number/  # Serial number build files
│   ├── simon_says/     # Simon Says build files
│   └── timer/          # Timer build files
├── firmware/           # Centralized UF2 files (created by build script)
├── build_all.ps1       # PowerShell build script
└── BUILD_README.md     # This file
```

## Troubleshooting

- **PlatformIO not found**: Ensure PlatformIO CLI is installed and in your PATH
- **Build failures**: Check individual module logs for specific error messages
- **No UF2 files**: Verify the build completed successfully and check the build directory structure
- **Permission errors**: Run as administrator if needed for file operations 