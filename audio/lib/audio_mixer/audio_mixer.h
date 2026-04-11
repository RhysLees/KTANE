#pragma once
#include <Arduino.h>
#include <AudioTools.h>
#include <amp.h>

// I2S pins feeding the TLV320DAC3100 (separate from I2C SDA/SCL on GP6/GP7)
constexpr uint8_t AUDIO_MIXER_PIN_BCK = 3;   // GP3 -> BCLK
constexpr uint8_t AUDIO_MIXER_PIN_WS  = 4;   // GP4 -> WSEL / LRCK
constexpr uint8_t AUDIO_MIXER_PIN_DOUT = 5;  // GP5 -> DIN


void initAudioMixer(uint8_t bckPin = AUDIO_MIXER_PIN_BCK,
                    uint8_t wsPin = AUDIO_MIXER_PIN_WS,
                    uint8_t dataPin = AUDIO_MIXER_PIN_DOUT);
void updateAudioMixer(); // Call in loop()
bool playSound(const int16_t* data, unsigned int length); // length in samples (not bytes)
bool playSoundFromFile(const char* filePath); // Stream large files from SD
bool audioMixerReady();
void setAudioMixerVolume(uint8_t volumePercent); // 0-100
uint8_t getAudioMixerVolume();
