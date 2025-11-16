#pragma once

#include <Arduino.h>
#include <cstddef>
#include <cstdint>

enum class SoundBankStatus : uint8_t {
  Success = 0,
  SdUnavailable,
  SoundNotRegistered,
  FileOpenFailed,
  FileReadError,
  FileTooLarge,  // File is too large to load into RAM, should be streamed
};

// Initialize internal bookkeeping. Requires the SD card to be available.
bool initSoundBank();

// Returns true when at least one sound has been successfully loaded.
bool soundBankReady();

// Clears any cached samples so they will be reloaded from SD on demand.
void soundBankResetCache();

// Returns a human readable description for debugging/logging.
const __FlashStringHelper* soundBankStatusToString(SoundBankStatus status);

// Ensures the requested audio message has sample data loaded from SD.
// On success, dataOut points to an internal buffer valid until the cache
// is reset. lengthOut is expressed in samples (int16_t frames).
SoundBankStatus getSoundFromBank(uint8_t audioMessageId,
                                 const int16_t** dataOut,
                                 size_t* lengthOut);

// Returns the absolute file path for the requested message, or nullptr if
// the message isn't registered in the bank.
const char* getSoundFilePath(uint8_t audioMessageId);


