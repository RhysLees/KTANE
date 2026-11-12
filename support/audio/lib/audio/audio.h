#pragma once
#include <Arduino.h>
#include <amp.h>

// I2S pins feeding the TLV320DAC3100
constexpr uint8_t AUDIO_PIN_BCK = 6;    // GP6 -> BCK
constexpr uint8_t AUDIO_PIN_WS  = 7;    // GP7 -> WSEL/LRCLK
constexpr uint8_t AUDIO_PIN_DOUT = 8;   // GP8 -> DIN

// Initialize the audio system (I2S output)
// Returns true on success
bool initAudio(uint8_t bckPin = AUDIO_PIN_BCK,
               uint8_t wsPin = AUDIO_PIN_WS,
               uint8_t dataPin = AUDIO_PIN_DOUT);

// Returns true if audio system is ready
bool audioReady();

// Play a WAV file from SD card (blocking)
// Returns true on success
bool playFile(const char* filePath);

