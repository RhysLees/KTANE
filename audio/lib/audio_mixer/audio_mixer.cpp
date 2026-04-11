#include <audio_mixer.h>
#include <hardware/sync.h>
#include <AudioTools.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <SdFat.h>
#include "sd_card.h"

// Must match TLV320 PLL + NDAC/MDAC (Adafruit example); KTANE_AUDIO WAVs are 44100 Hz.
#define SAMPLE_RATE 44100
#define MAX_VOICES 6
#define MAX_FILE_VOICES 6
#define BUFFER_SAMPLES 256
// RP2040 AudioTools: cfg.channels==2 uses int32-packed frames, not int16 L,R.
// Use channels==1 so writeBytes() uses writeExpandChannel → write16(L,R) per
// sample (correct for TLV320 I2S stereo slots). Software stays mono; driver
// duplicates to both slots.
constexpr int kI2sLogicalChannels = 1;

struct MixerVoice {
  const int16_t* data;
  size_t length;
  size_t position;
  bool active;
};

struct MixerFileVoice {
  FatFile file;
  uint32_t remaining = 0;
  unsigned frameBytes = 0;
  uint16_t numChannels = 0;
  bool active = false;
};

static MixerVoice voices[MAX_VOICES];
static MixerFileVoice fileVoices[MAX_FILE_VOICES];
static I2SStream i2s;  // AudioTools I2SStream for I2S output
static I2SConfig i2sConfig;
static AudioInfo audioInfo;
static volatile bool initialized = false;
static bool lockInitialized = false;
static critical_section_t mixerLock;
static uint8_t masterVolumePercent = 20;

static int32_t mixAccumulator[BUFFER_SAMPLES];
// Mono PCM fed to I2SStream; driver expands each sample to L+R on the wire.
static int16_t outputBuffer[BUFFER_SAMPLES];
// SD read staging: up to BUFFER_SAMPLES stereo PCM frames from file (4 bytes each).
static uint8_t fileReadChunk[BUFFER_SAMPLES * 4];
static uint8_t s_i2sBck = 0;
static uint8_t s_i2sWs = 0;
static uint8_t s_i2sData = 0;

extern FatVolume volume;

namespace {

struct WavInfo {
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
  uint16_t numChannels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
};

bool readBytesExactly(FatFile& file, void* buffer, size_t len) {
  return file.read(reinterpret_cast<uint8_t*>(buffer), len) ==
         static_cast<int>(len);
}

uint32_t readLE32(const uint8_t* data) {
  return static_cast<uint32_t>(data[0]) |
         (static_cast<uint32_t>(data[1]) << 8) |
         (static_cast<uint32_t>(data[2]) << 16) |
         (static_cast<uint32_t>(data[3]) << 24);
}

uint16_t readLE16(const uint8_t* data) {
  return static_cast<uint16_t>(data[0]) |
         (static_cast<uint16_t>(data[1]) << 8);
}

bool parseWavHeader(FatFile& file, WavInfo& info) {
  if (!file.seekSet(0)) {
    return false;
  }
  uint8_t header[12];
  if (!readBytesExactly(file, header, sizeof(header))) {
    return false;
  }
  if (memcmp(header, "RIFF", 4) != 0 || memcmp(header + 8, "WAVE", 4) != 0) {
    return false;
  }

  bool foundFmt = false;
  bool foundData = false;

  while (file.available32() > 0) {
    uint8_t chunkHeader[8];
    if (!readBytesExactly(file, chunkHeader, sizeof(chunkHeader))) {
      return false;
    }

    const uint32_t chunkSize = readLE32(chunkHeader + 4);
    const bool oddSize = chunkSize & 1;
    // First byte of chunk payload — nextChunkPos must not include bytes already
    // consumed reading fmt (fixes alignment after fmt for all standard WAVs).
    const uint32_t chunkPayloadStart = file.curPosition();
    const uint32_t nextChunkPos = chunkPayloadStart + chunkSize + (oddSize ? 1 : 0);

    if (memcmp(chunkHeader, "fmt ", 4) == 0) {
      if (chunkSize < 16) {
        return false;
      }
      uint8_t fmt[16];
      if (!readBytesExactly(file, fmt, sizeof(fmt))) {
        return false;
      }
      const uint16_t audioFormat = readLE16(fmt + 0);
      info.numChannels = readLE16(fmt + 2);
      info.sampleRate = readLE32(fmt + 4);
      info.bitsPerSample = readLE16(fmt + 14);
      if (audioFormat != 1) {
        return false;
      }
      foundFmt = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      info.dataOffset = file.curPosition();
      info.dataSize = chunkSize;
      foundData = true;
    }

    if (!foundFmt || !foundData) {
      if (!file.seekSet(nextChunkPos)) {
        return false;
      }
    } else {
      break;
    }
  }

  if (!foundFmt || !foundData || info.dataSize == 0) {
    return false;
  }
  if ((info.numChannels != 1 && info.numChannels != 2) ||
      info.bitsPerSample != 16) {
    return false;
  }
  return true;
}

}  // namespace

static uint8_t clampVolume(uint8_t percent) {
  return percent > 100 ? 100 : percent;
}

static void resetVoice(MixerVoice& voice) {
  voice.data = nullptr;
  voice.length = 0;
  voice.position = 0;
  voice.active = false;
}

static void resetFileVoice(MixerFileVoice& v) {
  v.file.close();
  v.remaining = 0;
  v.frameBytes = 0;
  v.numChannels = 0;
  v.active = false;
}

bool audioMixerReady() {
  return initialized;
}

void initAudioMixer(uint8_t bckPin, uint8_t wsPin, uint8_t dataPin) {
  if (initialized) return;

  s_i2sBck = bckPin;
  s_i2sWs = wsPin;
  s_i2sData = dataPin;

  if (!lockInitialized) {
    critical_section_init(&mixerLock);
    lockInitialized = true;
  }

  const bool codecPhase1Ok = initAmpPhaseBeforeI2s();
  if (!codecPhase1Ok) {
    KTANE_CONSOLE_OUT.println(
        F("[Audio] Codec phase1 failed. Fix I2C (see scan above); skipping codec phase2."));
  }

  audioInfo = AudioInfo(SAMPLE_RATE, kI2sLogicalChannels, 16);

  // Start BCLK/LRCK before powering codec PLL (PLL reference = BCLK).
  i2sConfig = i2s.defaultConfig(TX_MODE);
  i2sConfig.copyFrom(audioInfo);
  i2sConfig.pin_bck = bckPin;
  i2sConfig.pin_ws = wsPin;
  i2sConfig.pin_data = dataPin;
  i2sConfig.buffer_size = BUFFER_SAMPLES * sizeof(int16_t);
  i2sConfig.buffer_count = 4;
  if (!i2s.begin(i2sConfig)) {
    KTANE_CONSOLE_OUT.println(F("[Audio] I2S begin failed."));
  }
  delay(50);

  if (codecPhase1Ok && !initAmpPhaseAfterI2s()) {
    KTANE_CONSOLE_OUT.println(F("[Audio] Codec phase2 failed. Continuing without DAC path."));
  }

  critical_section_enter_blocking(&mixerLock);
  for (auto& voice : voices) {
    resetVoice(voice);
  }
  for (auto& fv : fileVoices) {
    resetFileVoice(fv);
  }
  critical_section_exit(&mixerLock);

  initialized = true;
}

bool playSound(const int16_t* data, unsigned int length) {
  if (!initialized || data == nullptr || length == 0) return false;

  bool queued = false;

  critical_section_enter_blocking(&mixerLock);
  for (auto& voice : voices) {
    if (!voice.active) {
      voice.data = data;
      voice.length = length;
      voice.position = 0;
      voice.active = true;
      queued = true;
      break;
    }
  }
  critical_section_exit(&mixerLock);

  return queued;
}

static void mixActiveVoices(bool& mixedSamples, bool& voicesRemaining) {
  std::fill(std::begin(mixAccumulator), std::end(mixAccumulator), 0);

  mixedSamples = false;
  voicesRemaining = false;

  for (auto& voice : voices) {
    if (!voice.active) continue;

    size_t samplesRemaining = (voice.length > voice.position)
                                  ? (voice.length - voice.position)
                                  : 0;
    if (samplesRemaining == 0) {
      resetVoice(voice);
      continue;
    }

    size_t samplesToMix =
        std::min(samplesRemaining, static_cast<size_t>(BUFFER_SAMPLES));
    const int16_t* src = voice.data + voice.position;

    for (size_t i = 0; i < samplesToMix; ++i) {
      mixAccumulator[i] += src[i];
    }

    voice.position += samplesToMix;
    mixedSamples = true;

    if (voice.position >= voice.length) {
      resetVoice(voice);
    } else {
      voicesRemaining = true;
    }
  }
}

static void mixFileVoices(bool& mixedSamples) {
  for (auto& fv : fileVoices) {
    if (!fv.active) {
      continue;
    }

    const unsigned frameBytes = fv.frameBytes;
    if (frameBytes == 0 || fv.remaining == 0) {
      resetFileVoice(fv);
      continue;
    }

    size_t chunkBytes =
        std::min(static_cast<size_t>(fv.remaining),
                 static_cast<size_t>(BUFFER_SAMPLES) * frameBytes);
    chunkBytes -= chunkBytes % frameBytes;
    if (chunkBytes == 0) {
      continue;
    }

    size_t totalRead = 0;
    while (totalRead < chunkBytes) {
      const int n =
          fv.file.read(fileReadChunk + totalRead, chunkBytes - totalRead);
      if (n <= 0) {
        resetFileVoice(fv);
        totalRead = 0;
        break;
      }
      totalRead += static_cast<size_t>(n);
    }
    totalRead -= totalRead % frameBytes;
    if (totalRead == 0) {
      continue;
    }

    if (fv.numChannels == 1) {
      const size_t monoSamples = totalRead / sizeof(int16_t);
      const int16_t* pcm = reinterpret_cast<const int16_t*>(fileReadChunk);
      for (size_t i = 0; i < monoSamples && i < BUFFER_SAMPLES; ++i) {
        mixAccumulator[i] += static_cast<int32_t>(pcm[i]);
      }
    } else {
      const size_t frames = totalRead / frameBytes;
      const int16_t* interleaved =
          reinterpret_cast<const int16_t*>(fileReadChunk);
      for (size_t f = 0; f < frames && f < BUFFER_SAMPLES; ++f) {
        const int32_t L = interleaved[f * 2];
        const int32_t R = interleaved[f * 2 + 1];
        mixAccumulator[f] += (L + R) / 2;
      }
    }

    fv.remaining -= static_cast<uint32_t>(totalRead);
    mixedSamples = true;

    if (fv.remaining == 0) {
      resetFileVoice(fv);
    }
  }
}

void updateAudioMixer() {
  if (!initialized) return;

  const size_t bufferBytes = BUFFER_SAMPLES * sizeof(int16_t);

  // Check if I2S output stream has space
  if (i2s.availableForWrite() < static_cast<int>(bufferBytes)) {
    return;
  }

  bool ramMixed = false;
  bool voicesRemaining = false;
  bool fileMixed = false;

  critical_section_enter_blocking(&mixerLock);
  mixActiveVoices(ramMixed, voicesRemaining);
  mixFileVoices(fileMixed);
  critical_section_exit(&mixerLock);
  (void)voicesRemaining;

  const bool mixedSamples = ramMixed || fileMixed;

  if (!mixedSamples) {
    std::memset(outputBuffer, 0, bufferBytes);
    i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
    return;
  }

  for (size_t i = 0; i < BUFFER_SAMPLES; ++i) {
    int32_t scaled = (mixAccumulator[i] * masterVolumePercent) / 100;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    outputBuffer[i] = static_cast<int16_t>(scaled);
  }

  i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
}

void setAudioMixerVolume(uint8_t volumePercent) {
  masterVolumePercent = clampVolume(volumePercent);
}

uint8_t getAudioMixerVolume() {
  return masterVolumePercent;
}

bool playSoundFromFile(const char* filePath) {
  if (!filePath || !initialized) {
    return false;
  }

  if (!sdCardReady() && !initSdCard()) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] SD card not available"));
    return false;
  }

  const char* relPath = filePath;
  if (relPath[0] == '/') {
    relPath++;
  }

  char volPath[128];
  if (relPath[0] == '\0') {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Empty path"));
    return false;
  }
  const int plen = snprintf(volPath, sizeof(volPath), "/%s", relPath);
  if (plen < 0 || static_cast<size_t>(plen) >= sizeof(volPath)) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Path too long"));
    return false;
  }

  critical_section_enter_blocking(&mixerLock);

  MixerFileVoice* slot = nullptr;
  for (auto& fv : fileVoices) {
    if (!fv.active) {
      slot = &fv;
      break;
    }
  }

  if (slot == nullptr) {
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] No free file stream (mixer busy)"));
    return false;
  }

  resetFileVoice(*slot);

  if (!slot->file.open(&volume, volPath, O_RDONLY)) {
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Failed to open file"));
    return false;
  }

  WavInfo wavInfo;
  if (!parseWavHeader(slot->file, wavInfo)) {
    resetFileVoice(*slot);
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.println(
        F("[Audio] [File] Invalid or unsupported WAV (need mono/stereo 16-bit PCM)"));
    return false;
  }

  if (!slot->file.seekSet(wavInfo.dataOffset)) {
    resetFileVoice(*slot);
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Seek failed"));
    return false;
  }

  if (wavInfo.sampleRate == 0) {
    resetFileVoice(*slot);
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Invalid sample rate"));
    return false;
  }

  const unsigned frameBytes =
      static_cast<unsigned>(wavInfo.numChannels) * sizeof(int16_t);
  if (frameBytes == 0 || (wavInfo.dataSize % frameBytes) != 0) {
    resetFileVoice(*slot);
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Bad WAV data size"));
    return false;
  }

  if (wavInfo.sampleRate != SAMPLE_RATE) {
    resetFileVoice(*slot);
    critical_section_exit(&mixerLock);
    KTANE_CONSOLE_OUT.print(F("[Audio] [File] WAV must be "));
    KTANE_CONSOLE_OUT.print(SAMPLE_RATE);
    KTANE_CONSOLE_OUT.println(F(" Hz (codec PLL fixed for this rate)."));
    return false;
  }

  slot->remaining = wavInfo.dataSize;
  slot->frameBytes = frameBytes;
  slot->numChannels = wavInfo.numChannels;
  slot->active = true;

  critical_section_exit(&mixerLock);

  KTANE_CONSOLE_OUT.print(F("[Audio] [File] Queued: "));
  KTANE_CONSOLE_OUT.println(filePath);
  return true;
}
