#include "amp.h"
#include <ktane_console.h>
#include <Wire.h>
#include <Adafruit_TLV320DAC3100.h>

static Adafruit_TLV320DAC3100 tlv320Codec;
static bool tlvCodecInitialized = false;

static bool tlv320RawWritePage(uint8_t page, uint8_t reg, uint8_t value) {
  Wire1.beginTransmission(TLV320DAC3100_I2CADDR_DEFAULT);
  Wire1.write(TLV320DAC3100_REG_PAGE_SELECT);
  Wire1.write(page);
  if (Wire1.endTransmission() != 0) {
    return false;
  }
  Wire1.beginTransmission(TLV320DAC3100_I2CADDR_DEFAULT);
  Wire1.write(reg);
  Wire1.write(value);
  return Wire1.endTransmission() == 0;
}

static bool checkCodecConfig(bool success, const __FlashStringHelper* message) {
  if (!success && message) {
    KTANE_CONSOLE_OUT.print(F("[Amp] [TLV320] "));
    KTANE_CONSOLE_OUT.print(message);
    KTANE_CONSOLE_OUT.println(F(" failed"));
  }
  return success;
}

static void tlv320I2cScan(TwoWire& w) {
  KTANE_CONSOLE_OUT.println(F("[Amp] [TLV320] I2C scan Wire1 (expect 0x18 or 0x19 for TLV320):"));
  int n = 0;
  for (uint8_t a = 1; a < 127; a++) {
    w.beginTransmission(a);
    if (w.endTransmission() == 0) {
      n++;
      KTANE_CONSOLE_OUT.print(F("  "));
      if (a < 16) {
        KTANE_CONSOLE_OUT.print('0');
      }
      KTANE_CONSOLE_OUT.println(a, HEX);
    }
  }
  if (n == 0) {
    KTANE_CONSOLE_OUT.println(F("  (no ACK on any address)"));
  }
}

static bool tlv320HardwareResetAndBus() {
  if (tlvCodecInitialized) {
    return true;
  }

#if TLV320_RESET_PIN >= 0
  pinMode(TLV320_RESET_PIN, OUTPUT);
  digitalWrite(TLV320_RESET_PIN, LOW);
  delay(100);
  digitalWrite(TLV320_RESET_PIN, HIGH);
  delay(100);
#endif

  KTANE_CONSOLE_OUT.println(F("[Amp] [TLV320] Configuring I2C pins..."));
  Wire1.setSDA(AUDIO_I2C_SDA_PIN);
  Wire1.setSCL(AUDIO_I2C_SCL_PIN);
  Wire1.begin();
  Wire1.setClock(100000);
  tlv320I2cScan(Wire1);

  KTANE_CONSOLE_OUT.println(F("[Amp] [TLV320] Starting codec.begin()..."));

  // Library default is &Wire (I2C0); this board uses Wire1 on GP6/GP7.
  if (!tlv320Codec.begin(TLV320DAC3100_I2CADDR_DEFAULT, &Wire1) &&
      !tlv320Codec.begin(0x19, &Wire1)) {
    KTANE_CONSOLE_OUT.println(
        F("[Amp] [TLV320] I2C probe failed (0x18/0x19 on Wire1). Check wiring, GND, and 3.3V."));
    return false;
  }
  KTANE_CONSOLE_OUT.println(F("[Amp] [TLV320] Codec detected over I2C."));
  return true;
}

// PLL is referenced from BCLK; program dividers first, then start I2S, then
// powerPLL (see initAmpPhaseAfterI2s). PLL/DOSR/NDAC/MDAC match
// TLV320_Audio_Playback_Arduino (44.1 kHz path).
bool initAmpPhaseBeforeI2s() {
  if (!tlv320HardwareResetAndBus()) {
    return false;
  }

  bool ok = true;
  ok &= checkCodecConfig(
      tlv320Codec.setCodecInterface(TLV320DAC3100_FORMAT_I2S,
                                    TLV320DAC3100_DATA_LEN_16),
      F("setCodecInterface"));
  ok &= checkCodecConfig(
      tlv320Codec.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL) &&
          tlv320Codec.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK),
      F("setClockInput"));
  ok &= checkCodecConfig(tlv320Codec.setPLLValues(1, 1, 8, 0),
                         F("setPLLValues"));
  ok &= checkCodecConfig(tlv320Codec.setNDAC(true, 8), F("setNDAC"));
  ok &= checkCodecConfig(tlv320Codec.setMDAC(true, 2), F("setMDAC"));
  ok &= checkCodecConfig(tlv320Codec.setDOSR(128), F("setDOSR"));

  if (!ok) {
    KTANE_CONSOLE_OUT.println(
        F("[Amp] [TLV320] Codec clock setup failed (before I2S)."));
    return false;
  }
  return true;
}

bool initAmpPhaseAfterI2s() {
  bool ok = true;
  ok &= checkCodecConfig(tlv320Codec.powerPLL(true), F("powerPLL"));
  delay(50);

  ok &= checkCodecConfig(tlv320Codec.setDACDataPath(
                             true, true, TLV320_DAC_PATH_NORMAL,
                             TLV320_DAC_PATH_NORMAL, TLV320_VOLUME_STEP_1SAMPLE),
                         F("setDACDataPath"));
  delay(5);
  ok &= checkCodecConfig(
      tlv320Codec.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER,
                                        TLV320_DAC_ROUTE_MIXER, false, false,
                                        false, false),
      F("configureAnalogInputs"));
  ok &= checkCodecConfig(
      tlv320Codec.setDACVolumeControl(false, false, TLV320_VOL_INDEPENDENT),
      F("setDACVolumeControl"));
  // setChannelVolume second arg is dB (-63.5..+24), not a raw register code.
  ok &= checkCodecConfig(tlv320Codec.setChannelVolume(false, 0.0f),
                         F("setChannelVolume L"));
  ok &= checkCodecConfig(tlv320Codec.setChannelVolume(true, 0.0f),
                         F("setChannelVolume R"));
  // Adafruit configureHeadphoneDriver omits writing HP_DRIVERS bit 2 (=1);
  // prime register so page-1 RMW succeeds on TLV320DAC3100.
  if (!tlv320RawWritePage(1, TLV320DAC3100_REG_HP_DRIVERS, 0x04)) {
    checkCodecConfig(false, F("prime HP_DRIVERS"));
    ok = false;
  }
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
    KTANE_CONSOLE_OUT.println(
        F("[Amp] [TLV320] Codec configuration failed (after I2S). Audio disabled."));
    return false;
  }

  tlvCodecInitialized = true;
  KTANE_CONSOLE_OUT.println(F("[Amp] [TLV320] Codec configured for I2S playback."));
  return true;
}

bool initAmp() {
  if (!initAmpPhaseBeforeI2s()) {
    return false;
  }
  if (!initAmpPhaseAfterI2s()) {
    return false;
  }
  return true;
}

bool ampReady() {
  return tlvCodecInitialized;
}

void resetAmp() {
  if (!tlvCodecInitialized) {
    KTANE_CONSOLE_OUT.println(F("[Amp] Amp not initialized. Cannot reset."));
    return;
  }

#if TLV320_RESET_PIN >= 0
  KTANE_CONSOLE_OUT.println(F("[Amp] Resetting amp via reset pin..."));
  digitalWrite(TLV320_RESET_PIN, LOW);
  delay(100);
  digitalWrite(TLV320_RESET_PIN, HIGH);
  delay(100);

  tlvCodecInitialized = false;
  if (initAmp()) {
    KTANE_CONSOLE_OUT.println(F("[Amp] Reset and reinitialization successful."));
  } else {
    KTANE_CONSOLE_OUT.println(F("[Amp] Reset successful but reinitialization failed."));
  }
#else
  KTANE_CONSOLE_OUT.println(F("[Amp] Reset pin not configured."));
#endif
}

bool setAmpHeadphoneVolume(bool rightChannel, uint8_t volume) {
  if (!tlvCodecInitialized) {
    KTANE_CONSOLE_OUT.println(F("[Amp] Amp not initialized. Cannot set volume."));
    return false;
  }

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
    KTANE_CONSOLE_OUT.print(F("[Amp] Headphone volume "));
    KTANE_CONSOLE_OUT.print(rightChannel ? F("R") : F("L"));
    KTANE_CONSOLE_OUT.print(F(" set to "));
    KTANE_CONSOLE_OUT.println(volume);
  } else {
    KTANE_CONSOLE_OUT.println(F("[Amp] Failed to set headphone volume."));
  }

  return success;
}

uint8_t getAmpHeadphoneVolume(bool rightChannel) {
  if (!tlvCodecInitialized) {
    return 0;
  }
  return 6;
}

bool setAmpSpeakerEnabled(bool enabled) {
  if (!tlvCodecInitialized) {
    KTANE_CONSOLE_OUT.println(F("[Amp] Amp not initialized. Cannot set speaker."));
    return false;
  }

  bool success = tlv320Codec.enableSpeaker(enabled);

  if (success) {
    KTANE_CONSOLE_OUT.print(F("[Amp] Speaker "));
    KTANE_CONSOLE_OUT.println(enabled ? F("enabled") : F("disabled"));
  } else {
    KTANE_CONSOLE_OUT.println(F("[Amp] Failed to set speaker state."));
  }

  return success;
}

bool getAmpSpeakerEnabled() {
  if (!tlvCodecInitialized) {
    return false;
  }
  return false;
}

void printAmpStatus() {
  KTANE_CONSOLE_OUT.println(F("[Amp] === Amp Status ==="));
  KTANE_CONSOLE_OUT.print(F("[Amp] Initialized: "));
  KTANE_CONSOLE_OUT.println(ampReady() ? F("Yes") : F("No"));

  if (ampReady()) {
    KTANE_CONSOLE_OUT.print(F("[Amp] I2C SDA: GP"));
    KTANE_CONSOLE_OUT.println(AUDIO_I2C_SDA_PIN);
    KTANE_CONSOLE_OUT.print(F("[Amp] I2C SCL: GP"));
    KTANE_CONSOLE_OUT.println(AUDIO_I2C_SCL_PIN);
#if TLV320_RESET_PIN >= 0
    KTANE_CONSOLE_OUT.print(F("[Amp] Reset pin: GP"));
    KTANE_CONSOLE_OUT.println(TLV320_RESET_PIN);
#else
    KTANE_CONSOLE_OUT.println(F("[Amp] Reset pin: Not configured"));
#endif
    KTANE_CONSOLE_OUT.print(F("[Amp] Speaker: "));
    KTANE_CONSOLE_OUT.println(getAmpSpeakerEnabled() ? F("Enabled") : F("Disabled"));
  }
  KTANE_CONSOLE_OUT.println(F("[Amp] ==================="));
}
