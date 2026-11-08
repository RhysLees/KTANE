// Core 1: SD read + WAV parse + push mono blocks into stream rings (EDGE_AUDIO audio module only).

#if defined(EDGE_AUDIO)

#include <audio_mixer.h>
#include <audio_mixer_ipc.h>
#include <stream_feeder.h>
#include <SdFat.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <ktane_console.h>
#include "sd_card.h"

extern FatVolume volume;

extern "C" void ktaneStreamFeederMarkDone(uint8_t slot);

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

struct StreamSource {
  FatFile file;
  uint32_t remaining = 0;
  unsigned frameBytes = 0;
  uint16_t numChannels = 0;
  bool active = false;
  uint8_t slot = 0;
};

static StreamSource g_sources[KTANE_AUDIO_STREAM_SLOTS];

static void closeSource(StreamSource& src) {
  src.file.close();
  src.remaining = 0;
  src.frameBytes = 0;
  src.numChannels = 0;
  src.active = false;
}

static bool ipcPushArmBlocking(uint8_t slot) {
  uint32_t spin = 0;
  while (audioMixerIpcTryPushStreamArm(slot) == 0) {
    delay(1);
    if (++spin > 500) {
      return false;
    }
  }
  return true;
}

static bool pushBlockBlocking(uint8_t slot, const int16_t* block) {
  uint32_t spin = 0;
  while (audioStreamRingTryPushBlock(slot, block) == 0) {
    delayMicroseconds(100);
    if (++spin > 100000) {
      return false;
    }
  }
  return true;
}

}  // namespace

void audioStreamFeederPump(void) {
  constexpr unsigned kBlock = KTANE_AUDIO_BLOCK_SAMPLES;

  for (uint8_t si = 0; si < KTANE_AUDIO_STREAM_SLOTS; ++si) {
    StreamSource& src = g_sources[si];
    if (!src.active) {
      continue;
    }

    int16_t block[kBlock];
    unsigned filled = 0;
    bool readError = false;

    while (filled < kBlock && src.remaining >= src.frameBytes) {
      if (src.numChannels == 1) {
        int16_t sample;
        if (src.file.read(&sample, sizeof(sample)) != sizeof(sample)) {
          readError = true;
          break;
        }
        src.remaining -= sizeof(sample);
        block[filled++] = sample;
      } else {
        int16_t lr[2];
        if (src.file.read(lr, sizeof(lr)) != sizeof(lr)) {
          readError = true;
          break;
        }
        src.remaining -= sizeof(lr);
        const int32_t m =
            (static_cast<int32_t>(lr[0]) + static_cast<int32_t>(lr[1])) / 2;
        block[filled++] = static_cast<int16_t>(m);
      }
    }

    if (readError) {
      closeSource(src);
      (void)audioMixerIpcTryPushStreamStop(si);
      ktaneStreamFeederMarkDone(si);
      continue;
    }

    while (filled < kBlock) {
      block[filled++] = 0;
    }

    if (!pushBlockBlocking(si, block)) {
      closeSource(src);
      (void)audioMixerIpcTryPushStreamStop(si);
      ktaneStreamFeederMarkDone(si);
      continue;
    }

    if (src.remaining == 0) {
      ktaneStreamFeederMarkDone(si);
      src.file.close();
      src.active = false;
      src.remaining = 0;
    }
  }
}

bool playSoundFromFile(const char* filePath) {
  if (!filePath || !audioMixerReady()) {
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

  uint8_t slot = 255;
  for (uint8_t s = 0; s < KTANE_AUDIO_STREAM_SLOTS; ++s) {
    if (audioMixerStreamSlotIsIdle(s) && !g_sources[s].active) {
      slot = s;
      break;
    }
  }
  if (slot == 255) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] No free stream slot"));
    return false;
  }

  FatFile probe;
  if (!probe.open(&volume, volPath, O_RDONLY)) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Failed to open file"));
    return false;
  }

  WavInfo wavInfo;
  if (!parseWavHeader(probe, wavInfo)) {
    probe.close();
    KTANE_CONSOLE_OUT.println(
        F("[Audio] [File] Invalid or unsupported WAV (need mono/stereo 16-bit PCM)"));
    return false;
  }
  probe.close();

  if (wavInfo.sampleRate != 44100) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] WAV must be 44100 Hz"));
    return false;
  }

  const unsigned frameBytes =
      static_cast<unsigned>(wavInfo.numChannels) * sizeof(int16_t);
  if (frameBytes == 0 || (wavInfo.dataSize % frameBytes) != 0) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Bad WAV data size"));
    return false;
  }

  StreamSource& src = g_sources[slot];
  closeSource(src);

  if (!src.file.open(&volume, volPath, O_RDONLY)) {
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Failed to open file"));
    return false;
  }

  if (!src.file.seekSet(wavInfo.dataOffset)) {
    closeSource(src);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] Seek failed"));
    return false;
  }

  src.remaining = wavInfo.dataSize;
  src.frameBytes = frameBytes;
  src.numChannels = wavInfo.numChannels;
  src.slot = slot;
  src.active = true;

  if (!ipcPushArmBlocking(slot)) {
    closeSource(src);
    KTANE_CONSOLE_OUT.println(F("[Audio] [File] IPC arm failed"));
    return false;
  }

  KTANE_CONSOLE_OUT.print(F("[Audio] [File] Queued: "));
  KTANE_CONSOLE_OUT.println(filePath);
  return true;
}

#else

// audio_console and other builds use playSoundFromFile in audio_mixer.cpp only.
void ktane_stream_feeder_unused_stub(void) {}

#endif  // EDGE_AUDIO
