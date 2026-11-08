---
name: arduino-audio-tools
description: >-
  Play PCM and WAV on the RP2040 with pschatzmann/arduino-audio-tools
  (I2SStream, WAVDecoder, AudioPlayer, AudioSourceSDFAT). Use when editing
  audio/, audio_test/, the mixer, or I2S buffer sizes. On this core I2S
  buffer_size is DMA words, not bytes.
---

# arduino-audio-tools

`https://github.com/pschatzmann/arduino-audio-tools.git` in `audio/` and `audio_test/`. The library detects `ARDUINO_ARCH_RP2040` without mbed and includes `AudioTools/PlatformConfig/rp2040hower.h`, which drives I2S through the Earle Philhower `I2S` class.

Include the umbrella header, or the specific pieces `audio_test` includes:

```cpp
#include <AudioTools.h>
#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/Disk/AudioSourceSDFAT.h"
```

Two playback paths exist:

- `audio/` mixes up to 6 RAM clips plus streamed WAV blocks and writes mono s16 to an `I2SStream`. That mixer is project code (`audio_mixer.cpp`), not the library's `OutputMixer`.
- `audio_test/` uses the library `AudioPlayer` end to end. Copy that pattern for a one-file bench test.

Codec bring-up is separate and must bookend `i2s.begin()`. See [adafruit-tlv320](../adafruit-tlv320/SKILL.md). SD access under the player uses [sdfat](../sdfat/SKILL.md).

## I2SStream

Pins on this bomb: BCLK GP3, word-select GP4, data out GP5. The codec is I2S master-clocked from BCLK; there is no MCLK wire.

```cpp
I2SStream i2s;
AudioInfo info(44100, /*channels*/ 1, /*bits*/ 16);
auto cfg = i2s.defaultConfig(TX_MODE);
cfg.copyFrom(info);
cfg.pin_bck = 3;
cfg.pin_ws = 4;
cfg.pin_data = 5;
cfg.buffer_size = 1536;    // DMA words per buffer, NOT bytes
cfg.buffer_count = 8;
if (!i2s.begin(cfg)) {
  // too-large buffers hang inside begin(); RAM is 264 KB
}
```

`buffer_size` reaches `I2S::setBuffers(count, words)`. `audio_test` uses 1536 words and 14 buffers and a 16 KB `AudioPlayer` copy buffer. The mixer path uses a smaller count because it writes its own blocks. Passing a byte count such as `44100 * 2 * 2` makes `begin()` allocate until the chip stalls.

`channels` is 1 in the mixer (`kI2sLogicalChannels`) and 2 in `audio_test`. The TLV320 still runs a stereo I2S format; a mono stream is a project choice so the mixer can add voices in one buffer. Match the WAV: a stereo file played as mono sounds like noise. `audio_mixer` rejects a WAV whose sample rate is not 44100.

`RX_MODE` is capture. This hardware has no I2S microphone wired.

After `begin`, `i2s.write(bytes, len)` pushes PCM. Call it often enough to keep the DMA buffers full. Underruns are clicks. `audio/` with `EDGE_AUDIO` runs `updateAudioMixer()` alone on core 0 for that reason, and runs CAN plus SD on core 1.

`i2s.end()` releases the PIO/DMA state. Do not call `begin` twice without `end`.

## AudioPlayer (the audio_test path)

```cpp
SdSpiConfig sdCfg(SD_CS, SHARED_SPI, SD_SCK_MHZ(25), &SPI1);
AudioSourceSDFAT<> source("/", "wav", sdCfg, /*repeat*/ true);
I2SStream i2s;
WAVDecoder decoder;
AudioPlayer player(source, i2s, decoder);

void setup() {
  // initAmpPhaseBeforeI2s(), then i2s.begin(cfg), then initAmpPhaseAfterI2s()
  source.setAutoNext(false);
  player.setBufferSize(16384);    // bytes, this one is bytes
  player.setDelayIfOutputFull(1);
  player.setVolume(0.2);          // 0.0 .. 1.0, applied in software
  player.begin(-1, false);       // mount SD, arm decoder, select no track yet
  player.setPath("sounds/strike.wav");
}
void loop1() {
  for (int i = 0; i < 24; i++) player.copy();
}
```

`AudioPlayer(AudioSource&, AudioOutput&, AudioDecoder&)` pulls encoded bytes from the source, decodes, and writes the sink. `copy()` does one chunk of that work and returns. It belongs in a tight loop. On the Pico W, keep it off the core that is also servicing USB or WiFi, or the stream stutters (upstream discussion 1884, linked from `audio_test/src/main.cpp`).

`begin(-1, false)` is what `audio_test` calls. Index `-1` selects no track, and a false return from that `begin()` still leaves the card mounted. `setPath` then opens one file, with no leading slash. `source.setAutoNext(false)` keeps the player from walking the folder. `setVolume` is linear amplitude. `begin()` runs after `i2s.begin`.

`AudioSourceSDFAT` constructors used in the wild:

- `AudioSourceSDFAT(const char* path, const char* ext, int cs, int speedMHz)`
- `AudioSourceSDFAT(const char* path, const char* ext, SdSpiConfig cfg, bool repeat)`

Use the `SdSpiConfig` form so the card stays on `&SPI1`. The `int chipSelect` form uses the default `SPI` object and collides with CAN.

`source.setFileFilter("*.wav")` and `source.selectStream(name)` pick among files. Paths for SdFat are relative to the volume root; strip a leading `/` before handing a path to `FatFile::open`. `audio_test` does that in `pathRelativeToVolume`.

## WAVDecoder

`WAVDecoder` parses a canonical PCM WAV header and then passes samples through. It handles 16-bit PCM. It is not a resampler. Feed it 44100 Hz files, which is what `sound_bank` stores.

```cpp
WAVDecoder decoder;
decoder.begin();
// AudioPlayer calls begin() itself
```

MP3 (`MP3DecoderHelix`) and AAC do not fit comfortably in 264 KB alongside the mixer, the CAN stack, and SdFat. Keep bomb sounds as 16-bit WAV.

`EncodedAudioOutput` plus a decoder is the lower-level form of the same pipeline when you already have a `Stream` and do not want `AudioPlayer`.

## Volume and mixing

The library `VolumeStream` wraps any `Print`. The bomb mixer does not use it; it scales int16 samples by a percent (`setAudioMixerVolume`, 0..100) while summing voices. If you insert `VolumeStream`, put it between the decoder and `I2SStream`, and leave the TLV320 headphone PGA where `amp.cpp` set it. Two software volumes in a row will clip.

`OutputMixer<int16_t>` exists in the library. This project does not use it, because streams are filled from the other core through a lock-free ring (`audio_mixer_ipc.h`). Add voices by extending that ring, not by constructing a second mixer.

## Logging

```cpp
AudioLogger::instance().begin(KTANE_CONSOLE_OUT, AudioLogger::Warning);
```

`Info` prints enough, often enough, to make WAV playback miss its deadline. Use `Warning` or `Error` while a sound is playing.

## Order of bring-up

Match `audio_test/src/main.cpp` and `audio/src/main.cpp`:

1. `initAmpPhaseBeforeI2s()` programs the TLV320 PLL dividers.
2. `i2s.begin(cfg)` starts BCLK.
3. `initAmpPhaseAfterI2s()` calls `powerPLL(true)`. The PLL reference is BCLK, so step 3 before step 2 yields a silent codec that still ACKs on I2C.
4. Then touch the SD card. SPI traffic during codec setup has been a source of failed inits.
