#include <audio_mixer.h>
#include <hardware/sync.h>

#define SAMPLE_RATE 22050
#define MAX_SOUNDS 4
#define BUFFER_SAMPLES 128

struct SoundInstance {
  const int16_t* data;
  unsigned int length;
  unsigned int index;
  bool active;
};

static SoundInstance sounds[MAX_SOUNDS];
static I2SStream i2s;
static I2SConfig i2sConfig;
static volatile bool initialized = false;
static bool loggerInitialized = false;
static bool lockInitialized = false;
static critical_section_t mixerLock;

bool audioMixerReady() {
  return initialized;
}

void initAudioMixer(uint8_t bckPin, uint8_t wsPin, uint8_t dataPin) {
  if (initialized) return;

  if (!lockInitialized) {
    critical_section_init(&mixerLock);
    lockInitialized = true;
  }

  if (!loggerInitialized) {
    AudioToolsLogger.begin(Serial, AudioToolsLogLevel::Warning);
    loggerInitialized = true;
  }

  AudioInfo info(SAMPLE_RATE, 1, 16);
  i2sConfig = i2s.defaultConfig(TX_MODE);
  i2sConfig.copyFrom(info);
  i2sConfig.pin_bck = bckPin;
  i2sConfig.pin_ws = wsPin;
  i2sConfig.pin_data = dataPin;
  i2sConfig.buffer_size = BUFFER_SAMPLES * sizeof(int16_t);
  i2sConfig.buffer_count = 4;
  i2s.begin(i2sConfig);

  critical_section_enter_blocking(&mixerLock);
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    sounds[i].data = nullptr;
    sounds[i].length = 0;
    sounds[i].index = 0;
    sounds[i].active = false;
  }
  critical_section_exit(&mixerLock);

  initialized = true;
}

bool playSound(const int16_t* data, unsigned int length) {
  if (!initialized || data == nullptr || length == 0) return false;

  bool queued = false;

  critical_section_enter_blocking(&mixerLock);
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (!sounds[i].active) {
      sounds[i].data = data;
      sounds[i].length = length;
      sounds[i].index = 0;
      sounds[i].active = true;
      queued = true;
      break;
    }
  }
  critical_section_exit(&mixerLock);

  return queued;
}

void updateAudioMixer() {
  if (!initialized) return;
  if (i2s.availableForWrite() < BUFFER_SAMPLES * sizeof(int16_t)) return;

  int16_t buffer[BUFFER_SAMPLES] = {0};

  critical_section_enter_blocking(&mixerLock);

  for (int i = 0; i < BUFFER_SAMPLES; ++i) {
    int32_t mixed = 0;
    int activeCount = 0;

    for (int s = 0; s < MAX_SOUNDS; ++s) {
      if (!sounds[s].active) continue;

      mixed += sounds[s].data[sounds[s].index++];
      activeCount++;

      if (sounds[s].index >= sounds[s].length) {
        sounds[s].active = false;
      }
    }

    if (activeCount > 0) mixed /= activeCount;
    buffer[i] = constrain(mixed, -32768, 32767);
  }

  critical_section_exit(&mixerLock);

  i2s.write(reinterpret_cast<const uint8_t*>(buffer), BUFFER_SAMPLES * sizeof(int16_t));
}

