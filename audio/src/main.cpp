#include <Arduino.h>
#include <can_bus.h>
#include <audio_mixer.h>
#include <module_state.h>
#include <sd_card.h>
#include <sound_bank.h>
#include <sd_test.h>
#include <amp.h>

static void processSerialCommand(char command);

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
  Serial.print(F("[Audio] "));
  Serial.print(source);
  Serial.print(F(" queued: "));
  Serial.println(sound);
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t *data, uint8_t len) {
  // Let module_state handle timer messages first
  moduleStateHandleCanMessage(id, senderId, data, len);
  
  // Handle audio-specific messages
  if (id == CAN_ID_AUDIO && len >= 1) {
    uint8_t messageId = data[0];

    // Handle audio control commands
    if (messageId == AUDIO_SET_VOLUME) {
      if (len >= 2) {
        uint8_t volume = data[1];
        setAudioMixerVolume(volume);
        Serial.print(F("[Audio] [CAN] Volume set to "));
        Serial.print(getAudioMixerVolume());
        Serial.println(F("%"));
      } else {
        Serial.println(F("[Audio] [CAN] Volume command missing payload."));
      }
      setModuleStateStatus(MODULE_STATUS_IDLE);
      return;
    }

    // Set module state to active when processing audio
    setModuleStateStatus(MODULE_STATUS_ACTIVE);

    const int16_t* soundData = nullptr;
    size_t soundLength = 0;
    SoundBankStatus bankStatus = getSoundFromBank(messageId, &soundData, &soundLength);

    if (bankStatus == SoundBankStatus::Success) {
      if (playSound(soundData, static_cast<unsigned int>(soundLength))) {
        logSoundQueued(F("[CAN]"), getSoundName(messageId));
      } else {
        Serial.println(F("[Audio] [CAN] Mixer busy, could not queue sound."));
      }
    } else if (bankStatus == SoundBankStatus::FileTooLarge) {
      // Stream large files instead of loading into RAM
      const char* path = getSoundFilePath(messageId);
      if (path && playSoundFromFile(path)) {
        logSoundQueued(F("[CAN] [Stream]"), getSoundName(messageId));
      } else {
        Serial.print(F("[Audio] [CAN] Failed to stream sound '"));
        Serial.println(getSoundName(messageId));
      }
    } else if (bankStatus == SoundBankStatus::SoundNotRegistered) {
      Serial.print(F("[Audio] [CAN] Unknown sound message id: 0x"));
      Serial.println(messageId, HEX);
    } else {
      Serial.print(F("[Audio] [CAN] Failed to load sound"));
      Serial.print(getSoundName(messageId));
      Serial.print(F(" ("));
      const char* path = getSoundFilePath(messageId);
      if (path) {
        Serial.print(path);
      } else {
        Serial.print(F("unmapped"));
      }
      Serial.print(F("): "));
      Serial.println(soundBankStatusToString(bankStatus));
    }
    
    // Return to idle status after processing
    setModuleStateStatus(MODULE_STATUS_IDLE);
  }
}

static void printSerialHelp() {
  Serial.println();
  Serial.println(F("[Audio] Serial test commands:"));
  Serial.println(F("  Sound playback:"));
  Serial.println(F("    1 - Normal beep"));
  Serial.println(F("    2 - Fast beep"));
  Serial.println(F("    3 - High beep"));
  Serial.println(F("    s - Strike"));
  Serial.println(F("    d - Defused"));
  Serial.println(F("    e - Explosion"));
  Serial.println(F("    f - Game over fanfare"));
  Serial.println(F("    a - Alarm clock beep"));
  Serial.println(F("    A - Alarm emergency"));
  Serial.println(F("    r - Simon red"));
  Serial.println(F("    b - Simon blue"));
  Serial.println(F("    g - Simon green"));
  Serial.println(F("    y - Simon yellow"));
  Serial.println(F("    t - Title theme"));
  Serial.println(F("  Volume control:"));
  Serial.println(F("    + / - - Volume up/down (5%)"));
  Serial.println(F("    = - Show current volume"));
  Serial.println(F("  Amp control:"));
  Serial.println(F("    m - Print amp status"));
  Serial.println(F("    M - Reset amp"));
  Serial.println(F("    [ / ] - Headphone volume L/R down/up (0-63)"));
  Serial.println(F("    { / } - Headphone volume both down/up (0-63)"));
  Serial.println(F("    k / K - Disable/enable speaker"));
  Serial.println(F("  Storage:"));
  Serial.println(F("    F - Show SD card info"));
  Serial.println(F("    L - List SD root directory"));
  Serial.println(F("    R - Reload sound files from SD"));
  Serial.println(F("  SD Card Tests:"));
  Serial.println(F("    T - Run full SD card test"));
  Serial.println(F("    B - Run basic SD card test"));
  Serial.println(F("    I - Print SD card details"));
  Serial.println(F("    W - Test SD card read/write"));
  Serial.println(F("  Help:"));
  Serial.println(F("    h or ? - Show this help"));
  Serial.println();
}

static void handleSerialInput() {
  while (Serial.available() > 0) {
    char command = Serial.read();
    if (command == '\r' || command == '\n') {
      continue;
    }
    processSerialCommand(command);
  }
}

static void processSerialCommand(char command) {
  const __FlashStringHelper* soundLabel = nullptr;
  uint8_t soundMessageId = 0;
  bool requestSoundPlayback = false;

  switch (command) {
      case '1':
        soundMessageId = AUDIO_PLAY_BEEP_NORMAL;
        requestSoundPlayback = true;
        break;
      case '2':
        soundMessageId = AUDIO_PLAY_BEEP_FAST;
        requestSoundPlayback = true;
        break;
      case '3':
        soundMessageId = AUDIO_PLAY_BEEP_HIGH;
        requestSoundPlayback = true;
        break;
      case 's':
        soundMessageId = AUDIO_PLAY_STRIKE;
        requestSoundPlayback = true;
        break;
      case 'd':
        soundMessageId = AUDIO_PLAY_DEFUSED;
        requestSoundPlayback = true;
        break;
      case 'e':
        soundMessageId = AUDIO_PLAY_EXPLODED;
        requestSoundPlayback = true;
        break;
      case 'f':
        soundMessageId = AUDIO_PLAY_GAME_OVER_FANFARE;
        requestSoundPlayback = true;
        break;
      case 'a':
        soundMessageId = AUDIO_PLAY_ALARM_CLOCK_BEEP;
        requestSoundPlayback = true;
        break;
      case 'A':
        soundMessageId = AUDIO_PLAY_ALARM_EMERGENCY;
        requestSoundPlayback = true;
        break;
      case 'r':
        soundMessageId = AUDIO_PLAY_SIMON_RED;
        requestSoundPlayback = true;
        break;
      case 'b':
        soundMessageId = AUDIO_PLAY_SIMON_BLUE;
        requestSoundPlayback = true;
        break;
      case 'g':
        soundMessageId = AUDIO_PLAY_SIMON_GREEN;
        requestSoundPlayback = true;
        break;
      case 'y':
        soundMessageId = AUDIO_PLAY_SIMON_YELLOW;
        requestSoundPlayback = true;
        break;
      case 't':
        // Check if this is a test command (basic test) or sound playback
        // For now, prioritize sound playback - use 'B' for basic test instead
        soundMessageId = AUDIO_PLAY_TITLE;
        requestSoundPlayback = true;
        break;
      case '+': {
        uint8_t current = getAudioMixerVolume();
        uint8_t next = current >= 95 ? 100 : current + 5;
        setAudioMixerVolume(next);
        Serial.print(F("[Audio] Volume set to "));
        Serial.print(next);
        Serial.println(F("%"));
        break;
      }
      case '-': {
        uint8_t current = getAudioMixerVolume();
        uint8_t next = current <= 5 ? 0 : current - 5;
        setAudioMixerVolume(next);
        Serial.print(F("[Audio] Volume set to "));
        Serial.print(next);
        Serial.println(F("%"));
        break;
      }
      case '=':
        Serial.print(F("[Audio] Volume "));
        Serial.print(getAudioMixerVolume());
        Serial.println(F("%"));
        break;
      case 'm':
        printAmpStatus();
        break;
      case 'M':
        resetAmp();
        break;
      case '[': {
        // Headphone volume L down
        uint8_t current = getAmpHeadphoneVolume(false);
        uint8_t next = current > 0 ? current - 1 : 0;
        setAmpHeadphoneVolume(false, next);
        break;
      }
      case ']': {
        // Headphone volume L up
        uint8_t current = getAmpHeadphoneVolume(false);
        uint8_t next = current < 63 ? current + 1 : 63;
        setAmpHeadphoneVolume(false, next);
        break;
      }
      case '{': {
        // Headphone volume both down
        uint8_t current = getAmpHeadphoneVolume(false);
        uint8_t next = current > 0 ? current - 1 : 0;
        setAmpHeadphoneVolume(false, next);
        setAmpHeadphoneVolume(true, next);
        break;
      }
      case '}': {
        // Headphone volume both up
        uint8_t current = getAmpHeadphoneVolume(false);
        uint8_t next = current < 63 ? current + 1 : 63;
        setAmpHeadphoneVolume(false, next);
        setAmpHeadphoneVolume(true, next);
        break;
      }
      case 'k':
        setAmpSpeakerEnabled(false);
        break;
      case 'K':
        setAmpSpeakerEnabled(true);
        break;
      case 'h':
      case '?':
        printSerialHelp();
        break;
      case 'F': {
        if (!sdCardReady() && !initSdCard()) {
          Serial.println(F("[SD] Initialization failed. See logs above."));
        } else {
          printSdCardInfo(Serial);
        }
        break;
      }
      case 'L': {
        if (!sdCardReady() && !initSdCard()) {
          Serial.println(F("[SD] Initialization failed. See logs above."));
        } else {
          listSdCardRoot(Serial);
        }
        break;
      }
      case 'R':
        soundBankResetCache();
        break;
      case 'T':
        runSdCardFullTest();
        break;
      case 'B':
        runSdCardBasicTest();
        break;
      case 'I':
        printSdCardDetails();
        break;
      case 'W':
        testSdCardReadWrite();
        break;
      case '\r':
      case '\n':
        break;
      default:
        Serial.print(F("[Audio] [Serial] Unknown command '"));
        Serial.print(command);
        Serial.println(F("'. Type 'h' for help."));
        break;
    }

    if (requestSoundPlayback) {
      const int16_t* soundData = nullptr;
      size_t soundLength = 0;
      SoundBankStatus status = getSoundFromBank(soundMessageId, &soundData, &soundLength);
      soundLabel = getSoundName(soundMessageId);
      if (status == SoundBankStatus::Success) {
        if (playSound(soundData, static_cast<unsigned int>(soundLength))) {
          logSoundQueued(F("[Serial]"), soundLabel);
        } else {
          Serial.println(F("[Audio] [Serial] Mixer busy, could not queue sound."));
        }
      } else if (status == SoundBankStatus::FileTooLarge) {
        // Stream large files instead of loading into RAM
        const char* path = getSoundFilePath(soundMessageId);
        if (path && playSoundFromFile(path)) {
          logSoundQueued(F("[Serial] [Stream]"), soundLabel);
        } else {
          Serial.print(F("[Audio] [Serial] Failed to stream sound '"));
          Serial.print(soundLabel);
          Serial.println(F("'"));
        }
      } else {
        Serial.print(F("[Audio] [Serial] Failed to load sound '"));
        Serial.print(soundLabel);
        Serial.print(F("' ("));
        const char* path = getSoundFilePath(soundMessageId);
        if (path) {
          Serial.print(path);
        } else {
          Serial.print(F("unmapped"));
        }
        Serial.print(F("): "));
        Serial.println(soundBankStatusToString(status));
      }
    }
}

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println();
  Serial.println(F("[Audio] Booting audio module..."));

  Serial.println(F("[Audio] Waiting 5 seconds before initialization..."));
  

  Serial.println(F("[Audio] TLV320: I2S BCLK=GP3 WSEL=GP4 DIN=GP5; I2C SDA=GP6 SCL=GP7"));

  Serial.println(F("[Audio] Initializing CAN bus..."));
  initCanBus(CAN_ID_AUDIO);
  registerCanCallback(onCanMessage);
  Serial.println(F("[Audio] CAN bus ready (ID=C0)"));
  
  // Initialize module_state system (starts in discovery mode)
  initModuleState(MODULE_STATE_NO_LED);  // Audio module doesn't have a status LED
  Serial.println(F("[Audio] Module state initialized"));

  Serial.println(F("[Audio] Starting mixer core..."));
  initAudioMixer();
  while (!audioMixerReady()) {
    delay(1);
  }
  Serial.println(F("[Audio] Mixer ready"));
  Serial.print(F("[Audio] Initial volume "));
  Serial.print(getAudioMixerVolume());
  Serial.println(F("%"));

  Serial.println(F("[Audio] Initializing SD card (SPI)..."));
  if (initSdCard()) {
    printSdCardInfo(Serial);
    if (!initSoundBank()) {
      Serial.println(F("[Audio] Sound bank initialization failed. Sounds will load once SD is ready."));
    }
  } else {
    Serial.println(F("[Audio] SD card not detected. Use 'F' to retry once inserted."));
  }

  Serial.println(F("[Audio] Ready. Type 'h' for help."));
}

void loop() {
  handleCanMessages();
  updateModuleState();
  handleSerialInput();
}

void setup1() {
  // Mixer is initialized in setup() on core 0
  // Core 1 just runs the update loop
  Serial.println(F("[Audio] Core1 mixer loop starting..."));
}

void loop1() {
  static bool loggedStart = false;
  if (!loggedStart) {
    Serial.println(F("[Audio] Core1 mixer loop active."));
    loggedStart = true;
  }

  updateAudioMixer();
}
