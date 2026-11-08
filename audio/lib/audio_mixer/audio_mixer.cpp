#include <audio_mixer.h>

#if defined(EDGE_AUDIO)

#include <AudioTools.h>
#include <algorithm>
#include <atomic>
#include <cstring>
#include <ktane_console.h>
#include <audio_mixer_ipc.h>

#define SAMPLE_RATE 44100
#define MAX_VOICES 6
#define MAX_STREAM_SLOTS KTANE_AUDIO_STREAM_SLOTS
#define BUFFER_SAMPLES KTANE_AUDIO_BLOCK_SAMPLES
constexpr int kI2sLogicalChannels = 1;

struct MixerVoice {
  const int16_t* data;
  size_t length;
  size_t position;
  bool active;
};

struct MixerStreamVoice {
  bool active = false;
};

static MixerVoice voices[MAX_VOICES];
static MixerStreamVoice streamVoices[MAX_STREAM_SLOTS];
static I2SStream i2s;
static I2SConfig i2sConfig;
static AudioInfo audioInfo;
static volatile bool initialized = false;

static std::atomic<uint8_t> g_volAtomic{20};

static int32_t mixAccumulator[BUFFER_SAMPLES];
static int16_t outputBuffer[BUFFER_SAMPLES];
static uint8_t s_i2sBck = 0;
static uint8_t s_i2sWs = 0;
static uint8_t s_i2sData = 0;

// --- IPC queue (core1 -> core0) ---
enum AudioIpcType : uint8_t {
  IPC_PLAY_RAM = 1,
  IPC_SET_VOLUME = 2,
  IPC_STREAM_ARM = 3,
  IPC_STREAM_STOP = 4,
};

struct AudioIpcMsg {
  uint8_t type;
  uint32_t a;
  uint32_t b;
};

static constexpr uint32_t kIpcCap = 32;
static AudioIpcMsg s_ipcQ[kIpcCap];
static std::atomic<uint32_t> s_ipcHead{0};
static std::atomic<uint32_t> s_ipcTail{0};

// --- Stream rings (core1 produces, core0 consumes) ---
static constexpr uint32_t kRingCap = 8;

struct StreamRing {
  std::atomic<uint32_t> head{0};
  std::atomic<uint32_t> tail{0};
  int16_t pcm[kRingCap][BUFFER_SAMPLES];
};

static StreamRing g_streamRings[MAX_STREAM_SLOTS];
static std::atomic<bool> g_streamFeedDone[MAX_STREAM_SLOTS];
static std::atomic<bool> g_streamSlotIdle[MAX_STREAM_SLOTS];

static uint8_t clampVolume(uint8_t percent) {
  return percent > 100 ? 100 : percent;
}

static void resetVoice(MixerVoice& voice) {
  voice.data = nullptr;
  voice.length = 0;
  voice.position = 0;
  voice.active = false;
}

static bool ipcTryPop(AudioIpcMsg* out) {
  uint32_t h = s_ipcHead.load(std::memory_order_relaxed);
  uint32_t t = s_ipcTail.load(std::memory_order_acquire);
  if (h == t) {
    return false;
  }
  *out = s_ipcQ[h % kIpcCap];
  s_ipcHead.store(h + 1, std::memory_order_release);
  return true;
}

static bool ipcTryPush(const AudioIpcMsg& m) {
  uint32_t t = s_ipcTail.load(std::memory_order_relaxed);
  uint32_t h = s_ipcHead.load(std::memory_order_acquire);
  if (t - h >= kIpcCap) {
    return false;
  }
  s_ipcQ[t % kIpcCap] = m;
  s_ipcTail.store(t + 1, std::memory_order_release);
  return true;
}

extern "C" {

void audioMixerIpcInit(void) {
  s_ipcHead.store(0, std::memory_order_relaxed);
  s_ipcTail.store(0, std::memory_order_relaxed);
}

int audioMixerIpcTryPushPlayRam(const int16_t* data, uint32_t length_samples) {
  if (!data || length_samples == 0) {
    return 0;
  }
  AudioIpcMsg m{};
  m.type = IPC_PLAY_RAM;
  m.a = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(data));
  m.b = length_samples;
  return ipcTryPush(m) ? 1 : 0;
}

int audioMixerIpcTryPushSetVolume(uint8_t percent) {
  AudioIpcMsg m{};
  m.type = IPC_SET_VOLUME;
  m.a = percent;
  return ipcTryPush(m) ? 1 : 0;
}

int audioMixerIpcTryPushStreamArm(uint8_t slot) {
  if (slot >= MAX_STREAM_SLOTS) {
    return 0;
  }
  AudioIpcMsg m{};
  m.type = IPC_STREAM_ARM;
  m.a = slot;
  return ipcTryPush(m) ? 1 : 0;
}

int audioMixerIpcTryPushStreamStop(uint8_t slot) {
  if (slot >= MAX_STREAM_SLOTS) {
    return 0;
  }
  AudioIpcMsg m{};
  m.type = IPC_STREAM_STOP;
  m.a = slot;
  return ipcTryPush(m) ? 1 : 0;
}

void audioStreamRingReset(uint8_t slot) {
  if (slot >= MAX_STREAM_SLOTS) {
    return;
  }
  g_streamRings[slot].head.store(0, std::memory_order_relaxed);
  g_streamRings[slot].tail.store(0, std::memory_order_relaxed);
  g_streamFeedDone[slot].store(false, std::memory_order_relaxed);
}

int audioStreamRingTryPushBlock(uint8_t slot, const int16_t* mono_block) {
  if (slot >= MAX_STREAM_SLOTS || !mono_block) {
    return 0;
  }
  StreamRing& r = g_streamRings[slot];
  const uint32_t h = r.head.load(std::memory_order_relaxed);
  const uint32_t t = r.tail.load(std::memory_order_acquire);
  if (h - t >= kRingCap) {
    return 0;
  }
  std::memcpy(r.pcm[h % kRingCap], mono_block,
              BUFFER_SAMPLES * sizeof(int16_t));
  r.head.store(h + 1, std::memory_order_release);
  return 1;
}

int audioMixerStreamSlotIsIdle(uint8_t slot) {
  if (slot >= MAX_STREAM_SLOTS) {
    return 0;
  }
  return g_streamSlotIdle[slot].load(std::memory_order_acquire) ? 1 : 0;
}

}  // extern "C"

static void drainMixerIpc(void) {
  AudioIpcMsg m;
  while (ipcTryPop(&m)) {
    switch (m.type) {
      case IPC_PLAY_RAM: {
        const int16_t* data =
            reinterpret_cast<const int16_t*>(static_cast<uintptr_t>(m.a));
        const uint32_t len = m.b;
        if (!data || len == 0) {
          break;
        }
        for (auto& voice : voices) {
          if (!voice.active) {
            voice.data = data;
            voice.length = len;
            voice.position = 0;
            voice.active = true;
            break;
          }
        }
        break;
      }
      case IPC_SET_VOLUME:
        g_volAtomic.store(clampVolume(static_cast<uint8_t>(m.a)),
                          std::memory_order_relaxed);
        break;
      case IPC_STREAM_ARM: {
        const uint8_t slot = static_cast<uint8_t>(m.a);
        if (slot < MAX_STREAM_SLOTS) {
          audioStreamRingReset(slot);
          streamVoices[slot].active = true;
          g_streamSlotIdle[slot].store(false, std::memory_order_release);
          g_streamFeedDone[slot].store(false, std::memory_order_relaxed);
        }
        break;
      }
      case IPC_STREAM_STOP: {
        const uint8_t slot = static_cast<uint8_t>(m.a);
        if (slot < MAX_STREAM_SLOTS) {
          streamVoices[slot].active = false;
          audioStreamRingReset(slot);
          g_streamSlotIdle[slot].store(true, std::memory_order_release);
        }
        break;
      }
      default:
        break;
    }
  }
}

static void mixActiveVoices(bool& mixedSamples, bool& voicesRemaining) {
  mixedSamples = false;
  voicesRemaining = false;

  for (auto& voice : voices) {
    if (!voice.active) {
      continue;
    }

    size_t samplesRemaining = (voice.length > voice.position)
                                  ? (voice.length - voice.position)
                                  : 0;
    if (samplesRemaining == 0) {
      resetVoice(voice);
      continue;
    }

    const size_t samplesToMix =
        std::min(samplesRemaining, static_cast<size_t>(BUFFER_SAMPLES));
    const int16_t* src = voice.data + voice.position;

    for (size_t i = 0; i < samplesToMix; ++i) {
      mixAccumulator[i] += static_cast<int32_t>(src[i]);
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

static bool streamRingPopIntoAccumulator(uint8_t slot) {
  StreamRing& r = g_streamRings[slot];
  const uint32_t t = r.tail.load(std::memory_order_relaxed);
  const uint32_t h = r.head.load(std::memory_order_acquire);
  if (t == h) {
    return false;
  }
  const int16_t* src = r.pcm[t % kRingCap];
  for (size_t i = 0; i < BUFFER_SAMPLES; ++i) {
    mixAccumulator[i] += static_cast<int32_t>(src[i]);
  }
  r.tail.store(t + 1, std::memory_order_release);
  return true;
}

static bool streamRingEmpty(uint8_t slot) {
  StreamRing& r = g_streamRings[slot];
  const uint32_t t = r.tail.load(std::memory_order_acquire);
  const uint32_t h = r.head.load(std::memory_order_acquire);
  return t == h;
}

static void mixStreamVoices(bool& mixedSamples) {
  for (uint8_t slot = 0; slot < MAX_STREAM_SLOTS; ++slot) {
    if (!streamVoices[slot].active) {
      continue;
    }
    if (streamRingPopIntoAccumulator(slot)) {
      mixedSamples = true;
      continue;
    }
    if (g_streamFeedDone[slot].load(std::memory_order_acquire) &&
        streamRingEmpty(slot)) {
      streamVoices[slot].active = false;
      g_streamFeedDone[slot].store(false, std::memory_order_relaxed);
      g_streamSlotIdle[slot].store(true, std::memory_order_release);
    }
  }
}

bool audioMixerReady() {
  return initialized;
}

void initAudioMixer(uint8_t bckPin, uint8_t wsPin, uint8_t dataPin) {
  if (initialized) {
    return;
  }

  s_i2sBck = bckPin;
  s_i2sWs = wsPin;
  s_i2sData = dataPin;

  audioMixerIpcInit();
  for (unsigned i = 0; i < MAX_STREAM_SLOTS; ++i) {
    audioStreamRingReset(static_cast<uint8_t>(i));
    g_streamSlotIdle[i].store(true, std::memory_order_release);
    streamVoices[i].active = false;
  }

  const bool codecPhase1Ok = initAmpPhaseBeforeI2s();
  if (!codecPhase1Ok) {
    KTANE_CONSOLE_OUT.println(
        F("[Audio] Codec phase1 failed. Fix I2C (see scan above); skipping codec phase2."));
  }

  audioInfo = AudioInfo(SAMPLE_RATE, kI2sLogicalChannels, 16);

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

  for (auto& voice : voices) {
    resetVoice(voice);
  }

  initialized = true;
}

bool playSound(const int16_t* data, unsigned int length) {
  if (!initialized || data == nullptr || length == 0) {
    return false;
  }
  return audioMixerIpcTryPushPlayRam(data, static_cast<uint32_t>(length)) != 0;
}

void updateAudioMixer() {
  if (!initialized) {
    return;
  }

  const size_t bufferBytes = BUFFER_SAMPLES * sizeof(int16_t);

  if (i2s.availableForWrite() < static_cast<int>(bufferBytes)) {
    return;
  }

  drainMixerIpc();

  std::fill(std::begin(mixAccumulator), std::end(mixAccumulator), 0);

  bool ramMixed = false;
  bool voicesRemaining = false;
  bool streamMixed = false;

  mixActiveVoices(ramMixed, voicesRemaining);
  mixStreamVoices(streamMixed);
  (void)voicesRemaining;

  const bool mixedSamples = ramMixed || streamMixed;
  const uint8_t vol = g_volAtomic.load(std::memory_order_relaxed);

  if (!mixedSamples) {
    std::memset(outputBuffer, 0, bufferBytes);
    i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
    return;
  }

  for (size_t i = 0; i < BUFFER_SAMPLES; ++i) {
    int32_t scaled = (mixAccumulator[i] * static_cast<int32_t>(vol)) / 100;
    if (scaled > 32767) {
      scaled = 32767;
    }
    if (scaled < -32768) {
      scaled = -32768;
    }
    outputBuffer[i] = static_cast<int16_t>(scaled);
  }

  i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
}

void setAudioMixerVolume(uint8_t volumePercent) {
  (void)audioMixerIpcTryPushSetVolume(clampVolume(volumePercent));
}

uint8_t getAudioMixerVolume() {
  return g_volAtomic.load(std::memory_order_relaxed);
}

// playSoundFromFile implemented in stream_feeder.cpp (EDGE_AUDIO).

extern "C" void ktaneStreamFeederMarkDone(uint8_t slot) {
  if (slot < MAX_STREAM_SLOTS) {
    g_streamFeedDone[slot].store(true, std::memory_order_release);
  }
}

#else  // !EDGE_AUDIO — single-core console build (legacy)

#include <AudioTools.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <SdFat.h>
#include <ktane_console.h>
#include "sd_card.h"

#define SAMPLE_RATE 44100
#define MAX_VOICES 6
#define MAX_FILE_VOICES 6
#define BUFFER_SAMPLES 256
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
static I2SStream i2s;
static I2SConfig i2sConfig;
static AudioInfo audioInfo;
static volatile bool initialized = false;
static uint8_t masterVolumePercent = 20;

static int32_t mixAccumulator[BUFFER_SAMPLES];
static int16_t outputBuffer[BUFFER_SAMPLES];
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
  if (initialized) {
    return;
  }

  s_i2sBck = bckPin;
  s_i2sWs = wsPin;
  s_i2sData = dataPin;

  audioSdLockInit();

  const bool codecPhase1Ok = initAmpPhaseBeforeI2s();
  if (!codecPhase1Ok) {
    KTANE_CONSOLE_OUT.println(
        F("[Audio] Codec phase1 failed. Fix I2C (see scan above); skipping codec phase2."));
  }

  audioInfo = AudioInfo(SAMPLE_RATE, kI2sLogicalChannels, 16);

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

  audioSdLock();
  for (auto& voice : voices) {
    resetVoice(voice);
  }
  for (auto& fv : fileVoices) {
    resetFileVoice(fv);
  }
  audioSdUnlock();

  initialized = true;
}

bool playSound(const int16_t* data, unsigned int length) {
  if (!initialized || data == nullptr || length == 0) {
    return false;
  }

  bool queued = false;

  audioSdLock();
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
  audioSdUnlock();

  return queued;
}

static void mixActiveVoices(bool& mixedSamples, bool& voicesRemaining) {
  std::fill(std::begin(mixAccumulator), std::end(mixAccumulator), 0);

  mixedSamples = false;
  voicesRemaining = false;

  for (auto& voice : voices) {
    if (!voice.active) {
      continue;
    }

    size_t samplesRemaining = (voice.length > voice.position)
                                  ? (voice.length - voice.position)
                                  : 0;
    if (samplesRemaining == 0) {
      resetVoice(voice);
      continue;
    }

    const size_t samplesToMix =
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
  if (!initialized) {
    return;
  }

  const size_t bufferBytes = BUFFER_SAMPLES * sizeof(int16_t);

  if (i2s.availableForWrite() < static_cast<int>(bufferBytes)) {
    return;
  }

  bool ramMixed = false;
  bool voicesRemaining = false;
  bool fileMixed = false;

  audioSdLock();
  mixActiveVoices(ramMixed, voicesRemaining);
  mixFileVoices(fileMixed);
  audioSdUnlock();
  (void)voicesRemaining;

  const bool mixedSamples = ramMixed || fileMixed;

  if (!mixedSamples) {
    std::memset(outputBuffer, 0, bufferBytes);
    i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
    return;
  }

  for (size_t i = 0; i < BUFFER_SAMPLES; ++i) {
    int32_t scaled = (mixAccumulator[i] * masterVolumePercent) / 100;
    if (scaled > 32767) {
      scaled = 32767;
    }
    if (scaled < -32768) {
      scaled = -32768;
    }
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

  audioSdLock();

  FatFile probe;
  if (!probe.open(&volume, volPath, O_RDONLY)) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Failed to open file"));
    audioSdUnlock();
    return false;
  }

  WavInfo wavInfo;
  if (!parseWavHeader(probe, wavInfo)) {
    probe.close();
    KTANE_CONSOLE_OUT.println(
        F("[Audio] [File] Invalid or unsupported WAV (need mono/stereo 16-bit PCM)"));
    audioSdUnlock();
    return false;
  }
  probe.close();

  if (wavInfo.sampleRate == 0) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Invalid sample rate"));
    audioSdUnlock();
    return false;
  }

  const unsigned frameBytes =
      static_cast<unsigned>(wavInfo.numChannels) * sizeof(int16_t);
  if (frameBytes == 0 || (wavInfo.dataSize % frameBytes) != 0) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Bad WAV data size"));
    audioSdUnlock();
    return false;
  }

  if (wavInfo.sampleRate != SAMPLE_RATE) {
    KTANE_CONSOLE_OUT.print(F("[Audio] [File] WAV must be "));
    KTANE_CONSOLE_OUT.print(SAMPLE_RATE);
    KTANE_CONSOLE_OUT.println(F(" Hz (codec PLL fixed for this rate)."));
    audioSdUnlock();
    return false;
  }

  MixerFileVoice* slot = nullptr;
  for (auto& fv : fileVoices) {
    if (!fv.active) {
      slot = &fv;
      break;
    }
  }

  if (slot == nullptr) {
    audioSdUnlock();
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] No free file stream (mixer busy)"));
    return false;
  }

  resetFileVoice(*slot);

  if (!slot->file.open(&volume, volPath, O_RDONLY)) {
    audioSdUnlock();
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Failed to open file"));
    return false;
  }

  if (!slot->file.seekSet(wavInfo.dataOffset)) {
    resetFileVoice(*slot);
    audioSdUnlock();
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Seek failed"));
    return false;
  }

  slot->remaining = wavInfo.dataSize;
  slot->frameBytes = frameBytes;
  slot->numChannels = wavInfo.numChannels;
  slot->active = true;

  audioSdUnlock();

  KTANE_CONSOLE_OUT.print(F("[Audio] [File] Queued: "));
  KTANE_CONSOLE_OUT.println(filePath);
  return true;
}

#endif  // EDGE_AUDIO
