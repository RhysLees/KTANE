#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>
#include <audio_mixer.h>
#include <module_state.h>
#include <sd_card.h>
#include <sound_bank.h>

static const __FlashStringHelper* getSoundName(uint8_t messageId) {
  switch (messageId) {
    case AUDIO_PLAY_BEEP_NORMAL: return F("Normal beep");
    case AUDIO_PLAY_BEEP_FAST: return F("Fast beep");
    case AUDIO_PLAY_BEEP_HIGH: return F("High beep");
    case AUDIO_PLAY_STRIKE: return F("Strike");
    case AUDIO_PLAY_DEFUSED: return F("Defused");
    case AUDIO_PLAY_EXPLODED: return F("Explosion");
    case AUDIO_PLAY_GAME_OVER_FANFARE: return F("Game over fanfare");
    case AUDIO_PLAY_ALARM_CLOCK_BEEP: return F("Alarm clock beep");
    case AUDIO_PLAY_ALARM_CLOCK_SNOOZE: return F("Alarm clock snooze");
    case AUDIO_PLAY_ALARM_EMERGENCY: return F("Alarm emergency");
    case AUDIO_PLAY_SIMON_RED: return F("Simon red");
    case AUDIO_PLAY_SIMON_BLUE: return F("Simon blue");
    case AUDIO_PLAY_SIMON_GREEN: return F("Simon green");
    case AUDIO_PLAY_SIMON_YELLOW: return F("Simon yellow");
    case AUDIO_PLAY_TITLE: return F("Title theme");
    default: return F("Unknown sound");
  }
}

static void logSoundQueued(const __FlashStringHelper* source, const __FlashStringHelper* sound) {
  KTANE_CONSOLE_OUT.print(F("[Audio] "));
  KTANE_CONSOLE_OUT.print(source);
  KTANE_CONSOLE_OUT.print(F(" queued: "));
  KTANE_CONSOLE_OUT.println(sound);
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  moduleStateHandleCanMessage(id, senderId, data, len);

  if (id == CAN_ID_AUDIO && len >= 1) {
    uint8_t messageId = data[0];

    if (messageId == AUDIO_SET_VOLUME) {
      if (len >= 2) {
        uint8_t volume = data[1];
        setAudioMixerVolume(volume);
        KTANE_CONSOLE_OUT.print(F("[Audio] [CAN] Volume set to "));
        KTANE_CONSOLE_OUT.print(getAudioMixerVolume());
        KTANE_CONSOLE_OUT.println(F("%"));
      } else {
        KTANE_CONSOLE_OUT.println(F("[Audio] [CAN] Volume command missing payload."));
      }
      setModuleStateStatus(MODULE_STATUS_IDLE);
      return;
    }

    setModuleStateStatus(MODULE_STATUS_ACTIVE);

    const int16_t* soundData = nullptr;
    size_t soundLength = 0;
    SoundBankStatus bankStatus = getSoundFromBank(messageId, &soundData, &soundLength);

    if (bankStatus == SoundBankStatus::Success) {
      if (playSound(soundData, static_cast<unsigned int>(soundLength))) {
        logSoundQueued(F("[CAN]"), getSoundName(messageId));
      } else {
        KTANE_CONSOLE_OUT.println(F("[Audio] [CAN] Mixer busy, could not queue sound."));
      }
    } else if (bankStatus == SoundBankStatus::FileTooLarge) {
      const char* path = getSoundFilePath(messageId);
      if (path && playSoundFromFile(path)) {
        logSoundQueued(F("[CAN] [Stream]"), getSoundName(messageId));
      } else {
        KTANE_CONSOLE_OUT.print(F("[Audio] [CAN] Failed to stream sound '"));
        KTANE_CONSOLE_OUT.println(getSoundName(messageId));
      }
    } else if (bankStatus == SoundBankStatus::SoundNotRegistered) {
      KTANE_CONSOLE_OUT.print(F("[Audio] [CAN] Unknown sound message id: 0x"));
      KTANE_CONSOLE_OUT.println(messageId, HEX);
    } else {
      KTANE_CONSOLE_OUT.print(F("[Audio] [CAN] Failed to load sound"));
      KTANE_CONSOLE_OUT.print(getSoundName(messageId));
      KTANE_CONSOLE_OUT.print(F(" ("));
      const char* path = getSoundFilePath(messageId);
      if (path) {
        KTANE_CONSOLE_OUT.print(path);
      } else {
        KTANE_CONSOLE_OUT.print(F("unmapped"));
      }
      KTANE_CONSOLE_OUT.print(F("): "));
      KTANE_CONSOLE_OUT.println(soundBankStatusToString(bankStatus));
    }

    setModuleStateStatus(MODULE_STATUS_IDLE);
  }
}

void setup() {
  ktaneConsoleInit(115200);
  delay(500);

  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println(F("[Audio] Booting audio module (CAN control)"));
  KTANE_CONSOLE_OUT.println(F("[Audio] I2S BCLK=GP3 WSEL=GP4 DIN=GP5; I2C SDA=GP6 SCL=GP7"));

  KTANE_CONSOLE_OUT.println(F("[Audio] Initializing CAN bus..."));
  initCanBus(CAN_ID_AUDIO);
  registerCanCallback(onCanMessage);
  KTANE_CONSOLE_OUT.println(F("[Audio] CAN bus ready (ID=C0)"));

  initModuleState(MODULE_STATE_NO_LED);
  KTANE_CONSOLE_OUT.println(F("[Audio] Module state initialized"));

  KTANE_CONSOLE_OUT.println(F("[Audio] Starting mixer / codec (I2S)..."));
  initAudioMixer();
  while (!audioMixerReady()) {
    delay(1);
  }
  KTANE_CONSOLE_OUT.print(F("[Audio] Mixer ready, volume "));
  KTANE_CONSOLE_OUT.print(getAudioMixerVolume());
  KTANE_CONSOLE_OUT.println(F("%"));

  KTANE_CONSOLE_OUT.println(F("[Audio] Initializing SD card (SPI)..."));
  if (initSdCard()) {
    printSdCardInfo(KTANE_CONSOLE_OUT);
    if (!initSoundBank()) {
      KTANE_CONSOLE_OUT.println(F("[Audio] Sound bank init failed; retry when SD contents are fixed."));
    }
  } else {
    KTANE_CONSOLE_OUT.println(F("[Audio] SD not ready; streaming sounds unavailable until a card is present."));
  }

  KTANE_CONSOLE_OUT.println(F("[Audio] Ready."));
}

void loop() {
  handleCanMessages();
  updateModuleState();
}

void setup1() {}

void loop1() {
  updateAudioMixer();
}
