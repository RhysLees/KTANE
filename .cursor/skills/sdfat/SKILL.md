---
name: sdfat
description: >-
  Read the audio module SD card with SdFat on SPI1 (SdSpiCard, FatVolume,
  FatFile, SdSpiConfig). Use when editing audio/lib/sd_card, sound_bank,
  stream_feeder, or audio_test file open. Lock the card before any call if
  both cores are running.
---

# SdFat

`<SdFat.h>` (Bill Greiman). It is not named in `lib_deps`. `arduino-audio-tools` brings it in for `AudioSourceSDFAT`, and `audio/lib/sd_card`, `sound_bank`, and `stream_feeder` include it directly. Do not add a second SdFat dependency with a different version.

The audio card is on SPI1, away from CAN on SPI0.

| Signal | GPIO |
|---|---|
| MISO | GP12 |
| CS | GP13 |
| SCK | GP14 |
| MOSI | GP15 |

Card format is FAT16 or FAT32. exFAT is a different `SdExFat` type; this code uses `FatVolume`.

## Init used by the sound bank

`sd_card.cpp` does not call `SdFat::begin`. It drives the card and the volume separately so it can retry the clock and then `chdir` itself. `FatVolume::init` does not set the working directory; `FatVolume::begin` would. The comment in `initSdCard` is the reason `volume.chdir()` is there. Keep it.

```cpp
#include <SdFat.h>
#include <SPI.h>

SdSpiCard card;
FatVolume volume;

SPI1.setRX(12);
SPI1.setTX(15);
SPI1.setSCK(14);
SPI1.begin();
pinMode(13, OUTPUT);
digitalWrite(13, HIGH);

SdSpiConfig config(/*cs*/ 13, SHARED_SPI, SD_SCK_MHZ(10), &SPI1);
if (!card.begin(config)) {
  SdSpiConfig slow(13, SHARED_SPI, SD_SCK_MHZ(1), &SPI1);
  if (!card.begin(slow)) {
    // no card, wiring, or not FAT
  }
}
if (!volume.init(&card)) {
  // partition is not FAT16/FAT32
}
if (!volume.chdir()) {
  // openRoot failed; later relative opens will fail
}
```

`SD_SCK_MHZ(n)` is the library macro. `audio_test` asks for 25 MHz once the wiring is known good. 10 MHz with a 1 MHz retry is the production default. `SHARED_SPI` is correct because the `SPI1` object is also used by the e-paper on other boards and by `AudioSourceSDFAT` on this one. `DEDICATED_SPI` would let SdFat reconfigure the bus exclusively; do not switch to it while `AudioPlayer` shares the same `SPIClass`.

The fourth constructor argument is the `SPIClass`. Without `&SPI1`, SdFat talks on SPI0 and the CAN controller brown-outs or the card never leaves idle.

## Files

Paths are relative to the volume working directory. A leading `/` is not stripped by `FatFile::open`. `audio_test` strips it before use. `sound_bank` paths should follow the same rule.

```cpp
FatFile file;
if (!file.open(&volume, "sounds/strike.wav", O_RDONLY)) {
  // missing, or chdir() was skipped
}
file.fileSize();
file.read(buf, n);
file.close();

file.getName(name, sizeof name);   // not 8.3 unless the file is 8.3
file.isDir();
file.rewind();
```

Directory walk, as in `listSdCardRoot`:

```cpp
FatFile dir;
dir.open(&volume, "/", O_RDONLY);
FatFile entry;
while (entry.openNext(&dir, O_RDONLY)) {
  entry.close();
}
```

`volume.exists(path)`, `volume.remove(path)`, `volume.mkdir(path)` work after `chdir`. Writes need `O_WRITE | O_CREAT`. The bomb never writes the card from firmware; sounds are copied on a PC.

`card.type()` values used by `printSdCardInfo`: 0 none, 1 MMC, 2 SDv1, 3 SDv2, 4 SDHC/SDXC.

## Two cores

SPI1 is not re-entrant, and FatFs keeps cursor state on the volume. `audioSdLock()` / `audioSdUnlock()` wrap a Pico SDK `mutex_t` (`pico/mutex.h`). Hold the lock across `open` + `read` + `close`, not just `begin`. `stream_feeder` and `sound_bank` already do this. A new reader on either core takes the same lock. `AudioPlayer` inside `audio_test` is the only thread using the card there, so that sketch does not take the mutex.

Initialize the mutex before core 1 starts work (`audioSdLockInit` from `initSdCard`'s caller). `mutex_enter_blocking` from an interrupt is illegal.

## AudioSourceSDFAT

For a library player, pass the same config rather than reimplementing `open`:

```cpp
SdSpiConfig cfg(13, SHARED_SPI, SD_SCK_MHZ(25), &SPI1);
AudioSourceSDFAT<> source("/", "wav", cfg, true);
```

That class calls `begin` itself. Do not also `card.begin` on the same pins in that sketch, or the two owners will both toggle CS.

## Failures

- `begin` fails at 10 MHz and works at 1 MHz: long wires. Ship the slower clock.
- `init` fails after `begin` succeeds: card is exFAT, or has no MBR partition. Reformat FAT32.
- `open` fails for a file the PC can see: path still has a leading `/`, or `chdir` did not run.
- Random SD errors only while audio plays: a second core is in SdFat without `audioSdLock`.
