#include "sound_bank.h"

#include <SdFat.h>
#include <vector>
#include <cstring>

#include "can_bus.h"
#include "sd_card.h"

// Access the global volume object from sd_card.cpp (matching working sdtest.cpp pattern)
extern FatVolume volume;

namespace {
struct SoundCacheEntry {
  uint8_t messageId;
  const char* filePath;
  std::vector<int16_t> samples;
  bool loaded;
};

#define SOUND_ENTRY(msg, filename) \
  { msg, filename, std::vector<int16_t>(), false }

SoundCacheEntry g_soundEntries[] = {
    SOUND_ENTRY(AUDIO_PLAY_TITLE, "/sounds/title.wav"),
    SOUND_ENTRY(AUDIO_PLAY_BEEP_NORMAL, "/sounds/double_beep.wav"),
    SOUND_ENTRY(AUDIO_PLAY_BEEP_FAST, "/sounds/double_beep_125.wav"),
    SOUND_ENTRY(AUDIO_PLAY_BEEP_HIGH, "/sounds/time_beep_high.wav"),
    SOUND_ENTRY(AUDIO_PLAY_STRIKE, "/sounds/strike.wav"),
    SOUND_ENTRY(AUDIO_PLAY_DEFUSED, "/sounds/bomb_defused.wav"),
    SOUND_ENTRY(AUDIO_PLAY_EXPLODED, "/sounds/explosion.wav"),
    SOUND_ENTRY(AUDIO_PLAY_CORRECT_TIME, "/sounds/correct_digital_chime.wav"),
    SOUND_ENTRY(AUDIO_PLAY_GAME_OVER_FANFARE, "/sounds/game_over_fanfare.wav"),
    SOUND_ENTRY(AUDIO_PLAY_ALARM_CLOCK_BEEP, "/sounds/alarm_clock_beep.wav"),
    SOUND_ENTRY(AUDIO_PLAY_ALARM_CLOCK_SNOOZE, "/sounds/alarm_clock_snooze.wav"),
    SOUND_ENTRY(AUDIO_PLAY_ALARM_EMERGENCY, "/sounds/alarm_emergency.wav"),
    SOUND_ENTRY(AUDIO_PLAY_SIMON_RED, "/sounds/simon_red.wav"),
    SOUND_ENTRY(AUDIO_PLAY_SIMON_GREEN, "/sounds/simon_green.wav"),
    SOUND_ENTRY(AUDIO_PLAY_SIMON_YELLOW, "/sounds/simon_yellow.wav"),
    SOUND_ENTRY(AUDIO_PLAY_SIMON_BLUE, "/sounds/simon_blue.wav"),
};

bool g_soundBankInitialized = false;

SoundCacheEntry* findEntry(uint8_t messageId) {
  for (auto& entry : g_soundEntries) {
    if (entry.messageId == messageId) {
      return &entry;
    }
  }
  return nullptr;
}

struct WavInfo {
  uint32_t dataOffset = 0;
  uint32_t dataSize = 0;
  uint16_t numChannels = 0;
  uint32_t sampleRate = 0;
  uint16_t bitsPerSample = 0;
};

bool readBytesExactly(FatFile& file, void* buffer, size_t len) {
  return file.read(reinterpret_cast<uint8_t*>(buffer), len) == len;
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
  // Reset to beginning of file
  if (!file.seekSet(0)) {
    Serial.println(F("[SoundBank] Failed to seek to file start"));
    return false;
  }
  
  uint8_t header[12];
  if (!readBytesExactly(file, header, sizeof(header))) {
    Serial.println(F("[SoundBank] Failed to read WAV header"));
    return false;
  }
  
  // Debug: print first 12 bytes
  Serial.print(F("[SoundBank] Header bytes: "));
  for (int i = 0; i < 12; i++) {
    Serial.print(header[i], HEX);
    Serial.print(F(" "));
  }
  Serial.println();
  
  if (memcmp(header, "RIFF", 4) != 0) {
    Serial.print(F("[SoundBank] Not a RIFF file. First 4 bytes: "));
    for (int i = 0; i < 4; i++) {
      Serial.print((char)header[i]);
    }
    Serial.println();
    return false;
  }
  
  if (memcmp(header + 8, "WAVE", 4) != 0) {
    Serial.print(F("[SoundBank] Not a WAVE file. Bytes 8-11: "));
    for (int i = 8; i < 12; i++) {
      Serial.print((char)header[i]);
    }
    Serial.println();
    return false;
  }

  bool foundFmt = false;
  bool foundData = false;

  while (file.available() > 0) {
    uint8_t chunkHeader[8];
    if (!readBytesExactly(file, chunkHeader, sizeof(chunkHeader))) {
      return false;
    }

    const uint32_t chunkSize = readLE32(chunkHeader + 4);
    const bool oddSize = chunkSize & 1;
    uint32_t nextChunkPos = file.curPosition() + chunkSize + (oddSize ? 1 : 0);

    if (memcmp(chunkHeader, "fmt ", 4) == 0) {
      if (chunkSize < 16) {
        Serial.print(F("[SoundBank] fmt chunk too small: "));
        Serial.println(chunkSize);
        return false;
      }
      uint8_t fmt[16];
      if (!readBytesExactly(file, fmt, sizeof(fmt))) {
        Serial.println(F("[SoundBank] Failed to read fmt chunk"));
        return false;
      }
      const uint16_t audioFormat = readLE16(fmt + 0);
      info.numChannels = readLE16(fmt + 2);
      info.sampleRate = readLE32(fmt + 4);
      info.bitsPerSample = readLE16(fmt + 14);

      Serial.print(F("[SoundBank] WAV format: "));
      Serial.print(F("format="));
      Serial.print(audioFormat);
      Serial.print(F(", channels="));
      Serial.print(info.numChannels);
      Serial.print(F(", rate="));
      Serial.print(info.sampleRate);
      Serial.print(F(", bits="));
      Serial.println(info.bitsPerSample);

      if (audioFormat != 1) {  // PCM
        Serial.print(F("[SoundBank] Unsupported audio format (not PCM): "));
        Serial.println(audioFormat);
        return false;
      }

      foundFmt = true;
    } else if (memcmp(chunkHeader, "data", 4) == 0) {
      info.dataOffset = file.curPosition();
      info.dataSize = chunkSize;
      foundData = true;
    }

    if (!foundFmt || !foundData) {
      file.seekSet(nextChunkPos);
    } else {
      break;
    }
  }

  if (!foundFmt) {
    Serial.println(F("[SoundBank] fmt chunk not found"));
    return false;
  }
  
  if (!foundData) {
    Serial.println(F("[SoundBank] data chunk not found"));
    return false;
  }

  if (info.numChannels != 1 || info.bitsPerSample != 16) {
    Serial.print(F("[SoundBank] Unsupported WAV format (need mono 16-bit, got "));
    Serial.print(info.numChannels);
    Serial.print(F(" channel(s), "));
    Serial.print(info.bitsPerSample);
    Serial.println(F(" bit)"));
    return false;
  }

  // Warn if sample rate is unusual (common rates: 44100, 22050, 48000)
  if (info.sampleRate != 44100 && info.sampleRate != 22050 && info.sampleRate != 48000) {
    Serial.print(F("[SoundBank] Warning: unusual sample rate "));
    Serial.print(info.sampleRate);
    Serial.println(F(" Hz (common: 44100/22050/48000). Playing anyway."));
  }

  return true;
}

SoundBankStatus loadEntryFromSd(SoundCacheEntry& entry) {
  if (!sdCardReady()) {
    if (!initSdCard()) {
      return SoundBankStatus::SdUnavailable;
    }
  }

  // Verify volume is initialized by trying to open root
  FatFile testRoot;
  if (!testRoot.openRoot(&volume)) {
    Serial.println(F("[SoundBank] Volume not initialized, reinitializing SD card..."));
    if (!initSdCard()) {
      return SoundBankStatus::SdUnavailable;
    }
    // Try again after reinitialization
    if (!testRoot.openRoot(&volume)) {
      Serial.println(F("[SoundBank] Failed to initialize volume."));
      return SoundBankStatus::SdUnavailable;
    }
  }
  testRoot.close();

  // Open file using root navigation (matching working sdtest.cpp pattern)
  FatFile root;
  if (!root.openRoot(&volume)) {
    Serial.println(F("[SoundBank] Failed to open root directory."));
    return SoundBankStatus::FileOpenFailed;
  }

  // Remove leading slash for relative path (FatFile.open expects relative paths)
  const char* relPath = entry.filePath;
  if (relPath[0] == '/') {
    relPath++;  // Skip leading slash
  }

  FatFile file;
  if (!file.open(&root, relPath, O_RDONLY)) {
    Serial.print(F("[SoundBank] Missing file: "));
    Serial.println(entry.filePath);
    root.close();
    return SoundBankStatus::FileOpenFailed;
  }
  
  root.close();  // Close root once file is open

  WavInfo wavInfo;
  if (!parseWavHeader(file, wavInfo)) {
    Serial.print(F("[SoundBank] Invalid WAV header: "));
    Serial.println(entry.filePath);
    file.close();
    return SoundBankStatus::FileReadError;
  }

  if (wavInfo.dataSize == 0) {
    Serial.print(F("[SoundBank] No audio data in "));
    Serial.println(entry.filePath);
    file.close();
    return SoundBankStatus::FileReadError;
  }

  if (wavInfo.dataSize % sizeof(int16_t) != 0) {
    Serial.print(F("[SoundBank] Warning: odd sample count in "));
    Serial.println(entry.filePath);
  }

  size_t sampleCount = static_cast<size_t>(wavInfo.dataSize / sizeof(int16_t));
  entry.samples.resize(sampleCount);

  if (!file.seekSet(wavInfo.dataOffset)) {
    Serial.print(F("[SoundBank] Failed to seek audio data for "));
    Serial.println(entry.filePath);
    entry.samples.clear();
    entry.loaded = false;
    file.close();
    return SoundBankStatus::FileReadError;
  }

  size_t bytesToRead = sampleCount * sizeof(int16_t);
  size_t totalRead = 0;
  uint8_t* bufferPtr = reinterpret_cast<uint8_t*>(entry.samples.data());

  while (totalRead < bytesToRead) {
    int chunk = file.read(bufferPtr + totalRead, bytesToRead - totalRead);
    if (chunk <= 0) {
      Serial.print(F("[SoundBank] Read error while loading "));
      Serial.println(entry.filePath);
      entry.samples.clear();
      entry.loaded = false;
      file.close();
      return SoundBankStatus::FileReadError;
    }
    totalRead += static_cast<size_t>(chunk);
  }

  file.close();
  entry.loaded = true;
  return SoundBankStatus::Success;
}
}  // namespace

bool initSoundBank() {
  if (g_soundBankInitialized) {
    return true;
  }

  if (!sdCardReady() && !initSdCard()) {
    Serial.println(F("[SoundBank] SD card not available."));
    return false;
  }

  g_soundBankInitialized = true;
  Serial.println(F("[SoundBank] Ready. Audio assets expected under sounds."));
  return true;
}

bool soundBankReady() {
  if (!g_soundBankInitialized) {
    return false;
  }
  for (const auto& entry : g_soundEntries) {
    if (entry.loaded) {
      return true;
    }
  }
  return false;
}

void soundBankResetCache() {
  for (auto& entry : g_soundEntries) {
    entry.samples.clear();
    entry.samples.shrink_to_fit();
    entry.loaded = false;
  }
  Serial.println(F("[SoundBank] Cache cleared. Sounds will reload on demand."));
}

const __FlashStringHelper* soundBankStatusToString(SoundBankStatus status) {
  switch (status) {
    case SoundBankStatus::Success:
      return F("OK");
    case SoundBankStatus::SdUnavailable:
      return F("SD unavailable");
    case SoundBankStatus::SoundNotRegistered:
      return F("Sound not registered");
    case SoundBankStatus::FileOpenFailed:
      return F("File open failed");
    case SoundBankStatus::FileReadError:
      return F("File read error");
  }
  return F("Unknown");
}

SoundBankStatus getSoundFromBank(uint8_t audioMessageId,
                                 const int16_t** dataOut,
                                 size_t* lengthOut) {
  if (!dataOut || !lengthOut) {
    return SoundBankStatus::FileReadError;
  }

  SoundCacheEntry* entry = findEntry(audioMessageId);
  if (!entry) {
    return SoundBankStatus::SoundNotRegistered;
  }

  // For large files (>1MB), we'll stream instead of loading into RAM
  // Check file size first
  FatFile root;
  if (root.openRoot(&volume)) {
    const char* relPath = entry->filePath;
    if (relPath[0] == '/') relPath++;
    
    FatFile testFile;
    if (testFile.open(&root, relPath, O_RDONLY)) {
      uint32_t fileSize = testFile.fileSize();
      testFile.close();
      root.close();
      
      // If file is larger than 1MB, return a special status to indicate streaming
      if (fileSize > 1024 * 1024) {
        // Return file path for streaming instead of data
        *dataOut = nullptr;
        *lengthOut = 0;
        return SoundBankStatus::FileTooLarge;  // We'll add this status
      }
    } else {
      root.close();
    }
  }

  if (!entry->loaded) {
    SoundBankStatus status = loadEntryFromSd(*entry);
    if (status != SoundBankStatus::Success) {
      return status;
    }
  }

  *dataOut = entry->samples.data();
  *lengthOut = entry->samples.size();
  return SoundBankStatus::Success;
}

const char* getSoundFilePath(uint8_t audioMessageId) {
  SoundCacheEntry* entry = findEntry(audioMessageId);
  return entry ? entry->filePath : nullptr;
}

