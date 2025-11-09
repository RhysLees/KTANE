#include <audio_mixer.h>
#include <hardware/sync.h>

#define SAMPLE_RATE 22050
#define MAX_SOUNDS 4
#define BUFFER_SAMPLES 256

struct MixerVoice {
  MemoryStream stream;
  bool active;
  int mixerIndex;
  uint8_t baseWeight;
};

static MixerVoice voices[MAX_SOUNDS];
static InputMixer<int16_t> mixer;
static StreamCopy copier;
static I2SStream i2s;
static I2SConfig i2sConfig;
static AudioInfo audioInfo;
static volatile bool initialized = false;
static bool loggerInitialized = false;
static bool lockInitialized = false;
static critical_section_t mixerLock;
static uint8_t masterVolumePercent = 100;

static uint8_t clampVolume(uint8_t percent) {
  return percent > 100 ? 100 : percent;
}

static void applyVoiceWeightUnlocked(int index) {
  if (index < 0 || index >= MAX_SOUNDS) return;
  const MixerVoice& voice = voices[index];
  if (!voice.active) {
    mixer.setWeight(voice.mixerIndex, 0);
    return;
  }

  uint16_t scaled = static_cast<uint16_t>(voice.baseWeight) * masterVolumePercent;
  uint8_t weight = static_cast<uint8_t>((scaled + 50) / 100); // round nearest
  mixer.setWeight(voice.mixerIndex, weight);
}

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

  audioInfo = AudioInfo(SAMPLE_RATE, 1, 16);

  i2sConfig = i2s.defaultConfig(TX_MODE);
  i2sConfig.copyFrom(audioInfo);
  i2sConfig.pin_bck = bckPin;
  i2sConfig.pin_ws = wsPin;
  i2sConfig.pin_data = dataPin;
  i2sConfig.buffer_size = BUFFER_SAMPLES * sizeof(int16_t);
  i2sConfig.buffer_count = 4;
  i2s.begin(i2sConfig);

  mixer.begin(audioInfo);
  mixer.setLimitToAvailableData(false);

  for (int i = 0; i < MAX_SOUNDS; ++i) {
    voices[i].stream.setAudioInfo(audioInfo);
    voices[i].stream.setValue(nullptr, 0);
    voices[i].stream.begin();
    voices[i].mixerIndex = mixer.add(voices[i].stream, 0);
    voices[i].active = false;
    voices[i].baseWeight = 100;
  }

  copier.begin(i2s, mixer);

  setAudioMixerVolume(masterVolumePercent);

  initialized = true;
}

bool playSound(const int16_t* data, unsigned int length) {
  if (!initialized || data == nullptr || length == 0) return false;

  const size_t byteLength = length * sizeof(int16_t);
  bool queued = false;

  critical_section_enter_blocking(&mixerLock);
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (!voices[i].active) {
      voices[i].stream.setValue(reinterpret_cast<const uint8_t*>(data), byteLength);
      voices[i].stream.begin();
      voices[i].baseWeight = 100;
      voices[i].active = true;
      applyVoiceWeightUnlocked(i);
      queued = true;
      break;
    }
  }
  critical_section_exit(&mixerLock);

  return queued;
}

void updateAudioMixer() {
  if (!initialized) return;

  critical_section_enter_blocking(&mixerLock);
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (voices[i].active && voices[i].stream.available() <= 0) {
      mixer.setWeight(voices[i].mixerIndex, 0);
      voices[i].stream.setValue(nullptr, 0);
      voices[i].stream.begin();
      voices[i].active = false;
    }
  }
  critical_section_exit(&mixerLock);

  copier.copy();
}

void setAudioMixerVolume(uint8_t volumePercent) {
  uint8_t newVolume = clampVolume(volumePercent);
  masterVolumePercent = newVolume;

  if (!initialized || !lockInitialized) return;

  critical_section_enter_blocking(&mixerLock);
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    applyVoiceWeightUnlocked(i);
  }
  critical_section_exit(&mixerLock);
}

uint8_t getAudioMixerVolume() {
  return masterVolumePercent;
}

