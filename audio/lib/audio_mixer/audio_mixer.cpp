#include <audio_mixer.h>
#include <PWMAudio.h>

#define SAMPLE_RATE 8000
#define MAX_SOUNDS 4
#define BUFFER_SAMPLES 128

struct SoundInstance {
  const int16_t* data;
  unsigned int length;
  unsigned int index;
  bool active;
};

static SoundInstance sounds[MAX_SOUNDS];
static PWMAudio pwm;
static bool initialized = false;

void initAudioMixer(uint8_t pin) {
  if (initialized) return;
  initialized = true;

  pwm = PWMAudio(pin);
  pwm.begin(SAMPLE_RATE);

  for (int i = 0; i < MAX_SOUNDS; ++i)
    sounds[i].active = false;
}

void playSound(const int16_t* data, unsigned int length) {
  for (int i = 0; i < MAX_SOUNDS; ++i) {
    if (!sounds[i].active) {
      sounds[i] = { data, length, 0, true };
      break;
    }
  }
}

void updateAudioMixer() {
  while (pwm.availableForWrite() >= BUFFER_SAMPLES * 2) {
    int16_t buffer[BUFFER_SAMPLES] = {0};

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

    pwm.write((const uint8_t*)buffer, BUFFER_SAMPLES * 2);
  }
}

