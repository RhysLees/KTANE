#pragma once
#include <Arduino.h>
#include <AudioTools.h>

constexpr uint8_t AUDIO_MIXER_PIN_BCK = 8;   // GP8
constexpr uint8_t AUDIO_MIXER_PIN_WS  = 9;   // GP9 (LRCLK)
constexpr uint8_t AUDIO_MIXER_PIN_DOUT = 7;  // GP7 (DIN on MAX98357A)

void initAudioMixer(uint8_t bckPin = AUDIO_MIXER_PIN_BCK,
                    uint8_t wsPin = AUDIO_MIXER_PIN_WS,
                    uint8_t dataPin = AUDIO_MIXER_PIN_DOUT);
void updateAudioMixer(); // Call in loop()
bool playSound(const int16_t* data, unsigned int length); // length in samples (not bytes)
bool audioMixerReady();
void setAudioMixerVolume(uint8_t volumePercent); // 0-100
uint8_t getAudioMixerVolume();
