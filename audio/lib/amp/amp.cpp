#include "amp.h"
#include <Wire.h>
#include <Adafruit_TLV320DAC3100.h>

static Adafruit_TLV320DAC3100 tlv320Codec;
static bool tlvCodecInitialized = false;

static bool checkCodecConfig(bool success, const __FlashStringHelper* message) {
  if (!success && message) {
    Serial.print(F("[Amp] [TLV320] "));
    Serial.print(message);
    Serial.println(F(" failed"));
  }
  return success;
}

bool initAmp() {
  if (tlvCodecInitialized) {
    return true;
  }

#if TLV320_RESET_PIN >= 0
  pinMode(TLV320_RESET_PIN, OUTPUT);
  digitalWrite(TLV320_RESET_PIN, LOW);
  delay(5);
  digitalWrite(TLV320_RESET_PIN, HIGH);
  delay(5);
#endif

  Serial.println(F("[Amp] [TLV320] Configuring I2C pins..."));
  Wire1.setSDA(AUDIO_I2C_SDA_PIN);
  Wire1.setSCL(AUDIO_I2C_SCL_PIN);
  Wire1.begin();
  Serial.println(F("[Amp] [TLV320] Starting codec.begin()..."));

  if (!tlv320Codec.begin()) {
    Serial.println(F("[Amp] [TLV320] Failed to initialize codec over I2C."));
    return false;
  }
  Serial.println(F("[Amp] [TLV320] Codec detected over I2C."));

  bool ok = true;
  ok &= checkCodecConfig(
      tlv320Codec.setCodecInterface(TLV320DAC3100_FORMAT_I2S,
                                    TLV320DAC3100_DATA_LEN_16),
      F("setCodecInterface"));
  ok &= checkCodecConfig(
      tlv320Codec.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) &&
          tlv320Codec.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK),
      F("setClockInput"));
  ok &= checkCodecConfig(tlv320Codec.setPLLValues(1, 2, 32, 0),
                         F("setPLLValues"));
  ok &= checkCodecConfig(tlv320Codec.setNDAC(true, 8), F("setNDAC"));
  ok &= checkCodecConfig(tlv320Codec.setMDAC(true, 2), F("setMDAC"));
  ok &= checkCodecConfig(tlv320Codec.powerPLL(true), F("powerPLL"));
  ok &= checkCodecConfig(tlv320Codec.setDACDataPath(
                             true, true, TLV320_DAC_PATH_NORMAL,
                             TLV320_DAC_PATH_NORMAL, TLV320_VOLUME_STEP_1SAMPLE),
                         F("setDACDataPath"));
  ok &= checkCodecConfig(
      tlv320Codec.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER,
                                        TLV320_DAC_ROUTE_MIXER, false, false,
                                        false, false),
      F("configureAnalogInputs"));
  ok &= checkCodecConfig(
      tlv320Codec.setDACVolumeControl(false, false, TLV320_VOL_INDEPENDENT),
      F("setDACVolumeControl"));
  ok &= checkCodecConfig(tlv320Codec.setChannelVolume(false, 18),
                         F("setChannelVolume L"));
  ok &= checkCodecConfig(tlv320Codec.setChannelVolume(true, 18),
                         F("setChannelVolume R"));
  ok &= checkCodecConfig(
      tlv320Codec.configureHeadphoneDriver(
          true, true, TLV320_HP_COMMON_1_35V, false) &&
          tlv320Codec.configureHPL_PGA(0, true) &&
          tlv320Codec.configureHPR_PGA(0, true) &&
          tlv320Codec.setHPLVolume(true, 6) &&
          tlv320Codec.setHPRVolume(true, 6),
      F("configureHeadphoneOutputs"));
  ok &= checkCodecConfig(
      tlv320Codec.enableSpeaker(false), F("disableSpeakerDefault"));

  if (!ok) {
    Serial.println(
        F("[Amp] [TLV320] Codec configuration failed. Audio disabled."));
    return false;
  }

  tlvCodecInitialized = true;
  Serial.println(F("[Amp] [TLV320] Codec configured for I2S playback."));
  return true;
}

bool ampReady() {
  return tlvCodecInitialized;
}

void resetAmp() {
  if (!tlvCodecInitialized) {
    Serial.println(F("[Amp] Amp not initialized. Cannot reset."));
    return;
  }

#if TLV320_RESET_PIN >= 0
  Serial.println(F("[Amp] Resetting amp via reset pin..."));
  digitalWrite(TLV320_RESET_PIN, LOW);
  delay(5);
  digitalWrite(TLV320_RESET_PIN, HIGH);
  delay(5);
  
  // Reinitialize after reset
  tlvCodecInitialized = false;
  if (initAmp()) {
    Serial.println(F("[Amp] Reset and reinitialization successful."));
  } else {
    Serial.println(F("[Amp] Reset successful but reinitialization failed."));
  }
#else
  Serial.println(F("[Amp] Reset pin not configured."));
#endif
}

bool setAmpHeadphoneVolume(bool rightChannel, uint8_t volume) {
  if (!tlvCodecInitialized) {
    Serial.println(F("[Amp] Amp not initialized. Cannot set volume."));
    return false;
  }

  // Clamp volume to valid range (0-63 for TLV320)
  if (volume > 63) {
    volume = 63;
  }

  bool success;
  if (rightChannel) {
    success = tlv320Codec.setHPRVolume(true, volume);
  } else {
    success = tlv320Codec.setHPLVolume(true, volume);
  }
  
  if (success) {
    Serial.print(F("[Amp] Headphone volume "));
    Serial.print(rightChannel ? F("R") : F("L"));
    Serial.print(F(" set to "));
    Serial.println(volume);
  } else {
    Serial.println(F("[Amp] Failed to set headphone volume."));
  }
  
  return success;
}

uint8_t getAmpHeadphoneVolume(bool rightChannel) {
  // Note: Adafruit_TLV320DAC3100 library doesn't expose a read function
  // We could maintain our own state, but for now return 0 if not ready
  if (!tlvCodecInitialized) {
    return 0;
  }
  
  // Default volume is 6 (from init). Could be extended to track state.
  // For now, return a placeholder or default
  return 6; // Default value from initialization
}

bool setAmpSpeakerEnabled(bool enabled) {
  if (!tlvCodecInitialized) {
    Serial.println(F("[Amp] Amp not initialized. Cannot set speaker."));
    return false;
  }

  bool success = tlv320Codec.enableSpeaker(enabled);
  
  if (success) {
    Serial.print(F("[Amp] Speaker "));
    Serial.println(enabled ? F("enabled") : F("disabled"));
  } else {
    Serial.println(F("[Amp] Failed to set speaker state."));
  }
  
  return success;
}

bool getAmpSpeakerEnabled() {
  if (!tlvCodecInitialized) {
    return false;
  }
  
  // Default is disabled (from init). Could be extended to track state.
  return false; // Default value from initialization
}

void printAmpStatus() {
  Serial.println(F("[Amp] === Amp Status ==="));
  Serial.print(F("[Amp] Initialized: "));
  Serial.println(ampReady() ? F("Yes") : F("No"));
  
  if (ampReady()) {
    Serial.print(F("[Amp] I2C SDA: GP"));
    Serial.println(AUDIO_I2C_SDA_PIN);
    Serial.print(F("[Amp] I2C SCL: GP"));
    Serial.println(AUDIO_I2C_SCL_PIN);
#if TLV320_RESET_PIN >= 0
    Serial.print(F("[Amp] Reset pin: GP"));
    Serial.println(TLV320_RESET_PIN);
#else
    Serial.println(F("[Amp] Reset pin: Not configured"));
#endif
    Serial.print(F("[Amp] Speaker: "));
    Serial.println(getAmpSpeakerEnabled() ? F("Enabled") : F("Disabled"));
    // Note: Volume readback not available without tracking state
  }
  Serial.println(F("[Amp] ==================="));
}

