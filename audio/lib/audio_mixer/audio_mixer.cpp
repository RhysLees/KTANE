#include <audio_mixer.h>
#include <hardware/sync.h>
#include <AudioTools.h>
#include <AudioTools/Disk/AudioSourceSDFAT.h>
#include <AudioTools/AudioCodecs/CodecWAV.h>
#include <algorithm>
#include <cstring>
#include <vector>
#include <SdFat.h>
#include "sd_card.h"

#define SAMPLE_RATE 22050
#define MAX_VOICES 6
#define BUFFER_SAMPLES 256

struct MixerVoice {
  const int16_t* data;
  size_t length;
  size_t position;
  bool active;
};

static MixerVoice voices[MAX_VOICES];
static I2SStream i2s;  // AudioTools I2SStream for I2S output
static I2SConfig i2sConfig;
static AudioInfo audioInfo;
static volatile bool initialized = false;
static bool lockInitialized = false;
static critical_section_t mixerLock;
static uint8_t masterVolumePercent = 20;

static int32_t mixAccumulator[BUFFER_SAMPLES];
static int16_t outputBuffer[BUFFER_SAMPLES];

static uint8_t clampVolume(uint8_t percent) {
  return percent > 100 ? 100 : percent;
}

static void resetVoice(MixerVoice& voice) {
  voice.data = nullptr;
  voice.length = 0;
  voice.position = 0;
  voice.active = false;
}

bool audioMixerReady() {
  return initialized;
}

void initAudioMixer(uint8_t bckPin, uint8_t wsPin, uint8_t dataPin) {
  if (initialized) return;

  if (!initAmp()) {
    Serial.println(F("[Audio] Codec init failed. Continuing without I2S output."));
  }

  if (!lockInitialized) {
    critical_section_init(&mixerLock);
    lockInitialized = true;
  }

  audioInfo = AudioInfo(SAMPLE_RATE, 1, 16);

  // Initialize I2S (default mode) using AudioTools I2SStream
  i2sConfig = i2s.defaultConfig(TX_MODE);
  i2sConfig.copyFrom(audioInfo);
  i2sConfig.pin_bck = bckPin;
  i2sConfig.pin_ws = wsPin;
  i2sConfig.pin_data = dataPin;
  i2sConfig.buffer_size = BUFFER_SAMPLES * sizeof(int16_t);
  i2sConfig.buffer_count = 4;
  i2s.begin(i2sConfig);

  critical_section_enter_blocking(&mixerLock);
  for (auto& voice : voices) {
    resetVoice(voice);
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

void updateAudioMixer() {
  if (!initialized) return;

  const size_t bufferBytes = BUFFER_SAMPLES * sizeof(int16_t);

  // Check if I2S output stream has space
  if (i2s.availableForWrite() < bufferBytes) {
    return;
  }

  bool mixedSamples = false;
  bool voicesRemaining = false;

  critical_section_enter_blocking(&mixerLock);
  mixActiveVoices(mixedSamples, voicesRemaining);
  critical_section_exit(&mixerLock);

  if (!mixedSamples) {
    // No active voices – send silence once and exit.
    std::memset(outputBuffer, 0, bufferBytes);
    i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
    return;
  }

  // Apply volume scaling
  for (size_t i = 0; i < BUFFER_SAMPLES; ++i) {
    int32_t scaled = (mixAccumulator[i] * masterVolumePercent) / 100;
    if (scaled > 32767) scaled = 32767;
    if (scaled < -32768) scaled = -32768;
    outputBuffer[i] = static_cast<int16_t>(scaled);
  }

  // Write to I2S output stream
  i2s.write(reinterpret_cast<const uint8_t*>(outputBuffer), bufferBytes);
}

void setAudioMixerVolume(uint8_t volumePercent) {
  masterVolumePercent = clampVolume(volumePercent);
}

uint8_t getAudioMixerVolume() {
  return masterVolumePercent;
}

// Access the global volume from sd_card.cpp
extern FatVolume volume;

bool playSoundFromFile(const char* filePath) {
  if (!filePath || !initialized) {
    return false;
  }

  // Ensure SD card is ready
  if (!sdCardReady() && !initSdCard()) {
    Serial.println(F("[Audio] [File] SD card not available"));
    return false;
  }

  Serial.print(F("[Audio] [File] Playing: "));
  Serial.println(filePath);

  // Extract directory and filename from path
  const char* filename = strrchr(filePath, '/');
  if (filename) {
    filename++; // Skip the '/'
  } else {
    filename = filePath;
  }

  // Get directory path (everything before the last '/')
  char dirPath[256];
  if (strrchr(filePath, '/')) {
    size_t dirLen = strrchr(filePath, '/') - filePath;
    strncpy(dirPath, filePath, dirLen);
    dirPath[dirLen] = '\0';
  } else {
    strcpy(dirPath, "/");
  }

  // Create audio source for WAV files from the directory
  AudioSourceSDFAT source(dirPath, "wav");
  
  // Create WAV decoder
  WAVDecoder decoder;
  
  // Create player with source, I2S output, and decoder
  AudioPlayer player(source, i2s, decoder);

  // Set up the player to play the specific file
  // AudioSourceSDFAT can filter by filename pattern
  // Use exact filename match (filename already extracted from path)
  source.setFileFilter(filename);

  // Begin the player
  if (!player.begin()) {
    Serial.println(F("[Audio] [File] Failed to begin player"));
    return false;
  }

  // Play the file by calling copy() in a loop until done
  // This is blocking, but necessary for proper playback
  unsigned long startTime = millis();
  const unsigned long MAX_PLAY_TIME = 60000; // 60 second timeout
  
  while (player.isActive()) {
    player.copy();
    
    // Safety timeout to prevent infinite loops
    if (millis() - startTime > MAX_PLAY_TIME) {
      Serial.println(F("[Audio] [File] Playback timeout"));
      player.stop();
      return false;
    }
    
    // Small delay to prevent CPU spinning
    delay(1);
  }

  Serial.println(F("[Audio] [File] Playback complete"));
  return true;
}
