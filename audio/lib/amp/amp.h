#pragma once
#include <Arduino.h>

// Default TLV320DAC3100 wiring (change if you use different pins)
constexpr uint8_t AUDIO_I2C_SDA_PIN = 6;   // GP6 -> SDA (I2C1)
constexpr uint8_t AUDIO_I2C_SCL_PIN = 7;   // GP7 -> SCL (I2C1)
constexpr int TLV320_RESET_PIN = 8;        // GP8 wired to TLV320 RST (active low)

// TLV320 PLL uses BCLK: program dividers first, run I2S, then finish (TI order).
bool initAmpPhaseBeforeI2s();
bool initAmpPhaseAfterI2s();

// Full init in one call (only safe if I2S already outputs BCLK, e.g. after reset).
bool initAmp();

// Returns true if the amp/codec has been successfully initialized.
bool ampReady();

// Reset the amp by toggling the reset pin
void resetAmp();

// Set headphone volume for left or right channel (0-63)
// Returns true on success
bool setAmpHeadphoneVolume(bool rightChannel, uint8_t volume);

// Get headphone volume for left or right channel (0-63)
// Returns 0 if amp not ready
uint8_t getAmpHeadphoneVolume(bool rightChannel);

// Enable or disable the speaker output
// Returns true on success
bool setAmpSpeakerEnabled(bool enabled);

// Get speaker enabled state
// Returns false if amp not ready
bool getAmpSpeakerEnabled();

// Print amp status to Serial
void printAmpStatus();

