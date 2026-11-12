---
name: module-audio-test
description: >-
  audio_test bench sketch: play one sound-bank WAV on the audio hardware
  through AudioPlayer and the TLV320, with RP2040 I2S buffer sizes. Use when
  editing audio_test/ or chasing SD stutter. Not a bomb module.
---

# audio_test

`support/audio_test/` is a Pico W bench sketch for the audio board. It is not on the bomb protocol. `lib_extra_dirs` includes `../shared_libs` and `../audio/lib`, so it links `sound_bank`, `sd_card`, `amp`, and `audio_mixer` headers for pins and paths.

`src/main.cpp` plays `AUDIO_PLAY_TITLE` (`/sounds/title.wav`) in a loop.

## What it does

1. `ktaneConsoleInit`.
2. `initAmpPhaseBeforeI2s()`.
3. `I2SStream` at 44100 Hz, 2 channels, 16-bit, pins GP3/GP4/GP5. `buffer_size = 1536` DMA words, `buffer_count = 14`.
4. `initAmpPhaseAfterI2s()` so the PLL locks to a real BCLK.
5. SPI1 SD pins from `sd_card.h`, `SdSpiConfig` at 25 MHz on `&SPI1`.
6. `AudioSourceSDFAT<>`, `WAVDecoder`, `AudioPlayer`. `setBufferSize(16384)` bytes, `setDelayIfOutputFull(1)`, `begin(-1, false)`, then `setPath` with the slash stripped. `copy()` runs on core 1, 24 calls per `loop1` pass.

Core 0 stays free for USB. That split, and the word-sized I2S buffers, are the fix tracked in arduino-audio-tools discussion 1884. A `buffer_size` in the tens of thousands hangs inside `I2S::begin` because the core treats it as words.

`getSoundFilePath` returns paths with a leading slash. `pathRelativeToVolume` strips one slash before SdFat sees them.

## When to change which sketch

Change `audio_test` to prove a file, a clock, or a buffer setting. Copy a setting into `audio/lib/audio_mixer` only after this sketch plays cleanly. The mixer is mono and writes its own blocks; pasting the 14-buffer stereo config across unchanged will blow RAM.

CAN is linked (`mcp_can` is in `lib_deps`) and unused by `main.cpp`. Leave the bus quiet while this sketch owns SPI1 and I2S, or run it on a board that is off the bus.
