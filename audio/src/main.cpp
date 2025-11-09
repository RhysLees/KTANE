#include <Arduino.h>
#include <can_bus.h>
#include <audio_mixer.h>
#include <sounds.h>
#include <module_state.h>

static const __FlashStringHelper* getSoundName(uint8_t messageId) {
  switch (messageId) {
    case AUDIO_BEEP_NORMAL: return F("Normal beep");
    case AUDIO_BEEP_FAST: return F("Fast beep");
    case AUDIO_BEEP_HIGH: return F("High beep");
    case AUDIO_STRIKE: return F("Strike");
    case AUDIO_DEFUSED: return F("Defused");
    case AUDIO_EXPLODED: return F("Explosion");
    case AUDIO_GAME_OVER_FANFARE: return F("Game over fanfare");
    case AUDIO_ALARM_CLOCK_BEEP: return F("Alarm clock beep");
    case AUDIO_ALARM_CLOCK_SNOOZE: return F("Alarm clock snooze");
    case AUDIO_ALARM_EMERGENCY: return F("Alarm emergency");
    case AUDIO_SIMON_RED: return F("Simon red");
    case AUDIO_SIMON_BLUE: return F("Simon blue");
    case AUDIO_SIMON_GREEN: return F("Simon green");
    case AUDIO_SIMON_YELLOW: return F("Simon yellow");
    case AUDIO_TITLE: return F("Title theme");
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
    
    // Set module state to active when processing audio
    setModuleStateStatus(MODULE_STATUS_ACTIVE);
    
    const int16_t* soundData = nullptr;
    unsigned int soundLength = 0;

    switch (messageId) {
      case AUDIO_BEEP_NORMAL:
        soundData = double_beep;
        soundLength = DOUBLE_BEEP_SAMPLES;
        break;
      case AUDIO_BEEP_FAST:
        soundData = double_beep_125;
        soundLength = DOUBLE_BEEP_125_SAMPLES;
        break;
      case AUDIO_BEEP_HIGH:
        soundData = time_beep_high;
        soundLength = TIME_BEEP_HIGH_SAMPLES;
        break;
      case AUDIO_STRIKE:
        soundData = strike;
        soundLength = STRIKE_SAMPLES;
        break;
      case AUDIO_DEFUSED:
        soundData = bomb_defused;
        soundLength = BOMB_DEFUSED_SAMPLES;
        break;
      case AUDIO_EXPLODED:
        soundData = explosion;
        soundLength = EXPLOSION_SAMPLES;
        break;
      case AUDIO_GAME_OVER_FANFARE:
        soundData = game_over_fanfare;
        soundLength = GAME_OVER_FANFARE_SAMPLES;
        break;
      case AUDIO_ALARM_CLOCK_BEEP:
        soundData = alarm_clock_beep;
        soundLength = ALARM_CLOCK_BEEP_SAMPLES;
        break;
      case AUDIO_ALARM_CLOCK_SNOOZE:
        soundData = alarm_clock_snooze;
        soundLength = ALARM_CLOCK_SNOOZE_SAMPLES;
        break;
      case AUDIO_ALARM_EMERGENCY:
        soundData = alarm_emergency;
        soundLength = ALARM_EMERGENCY_SAMPLES;
        break;
      case AUDIO_SIMON_RED:
        soundData = simon_red;
        soundLength = SIMON_RED_SAMPLES;
        break;
      case AUDIO_SIMON_BLUE:
        soundData = simon_blue;
        soundLength = SIMON_BLUE_SAMPLES;
        break;
      case AUDIO_SIMON_GREEN:
        soundData = simon_green;
        soundLength = SIMON_GREEN_SAMPLES;
        break;
      case AUDIO_SIMON_YELLOW:
        soundData = simon_yellow;
        soundLength = SIMON_YELLOW_SAMPLES;
        break;
      case AUDIO_TITLE:
        soundData = title;
        soundLength = TITLE_SAMPLES;
        break;
      default:
        Serial.print(F("[Audio] [CAN] Unknown message id: 0x"));
        Serial.println(messageId, HEX);
        break;
    }

    if (soundData != nullptr) {
      if (playSound(soundData, soundLength)) {
        logSoundQueued(F("[CAN]"), getSoundName(messageId));
      } else {
        Serial.println(F("[Audio] [CAN] Mixer busy, could not queue sound."));
      }
    }
    
    // Return to idle status after processing
    setModuleStateStatus(MODULE_STATUS_IDLE);
  }
}

static void printSerialHelp() {
  Serial.println();
  Serial.println(F("[Audio] Serial test commands:"));
  Serial.println(F("  1 - Normal beep"));
  Serial.println(F("  2 - Fast beep"));
  Serial.println(F("  3 - High beep"));
  Serial.println(F("  s - Strike"));
  Serial.println(F("  d - Defused"));
  Serial.println(F("  e - Explosion"));
  Serial.println(F("  f - Game over fanfare"));
  Serial.println(F("  a - Alarm clock beep"));
  Serial.println(F("  A - Alarm emergency"));
  Serial.println(F("  r - Simon red"));
  Serial.println(F("  b - Simon blue"));
  Serial.println(F("  g - Simon green"));
  Serial.println(F("  y - Simon yellow"));
  Serial.println(F("  t - Title theme"));
  Serial.println(F("  h or ? - Show this help"));
  Serial.println();
}

static void handleSerialInput() {
  while (Serial.available() > 0) {
    char command = static_cast<char>(Serial.read());

    const int16_t* soundData = nullptr;
    unsigned int soundLength = 0;
    const __FlashStringHelper* soundLabel = nullptr;

    switch (command) {
      case '1':
        soundData = double_beep;
        soundLength = DOUBLE_BEEP_SAMPLES;
        soundLabel = F("Normal beep");
        break;
      case '2':
        soundData = double_beep_125;
        soundLength = DOUBLE_BEEP_125_SAMPLES;
        soundLabel = F("Fast beep");
        break;
      case '3':
        soundData = time_beep_high;
        soundLength = TIME_BEEP_HIGH_SAMPLES;
        soundLabel = F("High beep");
        break;
      case 's':
        soundData = strike;
        soundLength = STRIKE_SAMPLES;
        soundLabel = F("Strike");
        break;
      case 'd':
        soundData = bomb_defused;
        soundLength = BOMB_DEFUSED_SAMPLES;
        soundLabel = F("Defused");
        break;
      case 'e':
        soundData = explosion;
        soundLength = EXPLOSION_SAMPLES;
        soundLabel = F("Explosion");
        break;
      case 'f':
        soundData = game_over_fanfare;
        soundLength = GAME_OVER_FANFARE_SAMPLES;
        soundLabel = F("Game over fanfare");
        break;
      case 'a':
        soundData = alarm_clock_beep;
        soundLength = ALARM_CLOCK_BEEP_SAMPLES;
        soundLabel = F("Alarm clock beep");
        break;
      case 'A':
        soundData = alarm_emergency;
        soundLength = ALARM_EMERGENCY_SAMPLES;
        soundLabel = F("Alarm emergency");
        break;
      case 'r':
        soundData = simon_red;
        soundLength = SIMON_RED_SAMPLES;
        soundLabel = F("Simon red");
        break;
      case 'b':
        soundData = simon_blue;
        soundLength = SIMON_BLUE_SAMPLES;
        soundLabel = F("Simon blue");
        break;
      case 'g':
        soundData = simon_green;
        soundLength = SIMON_GREEN_SAMPLES;
        soundLabel = F("Simon green");
        break;
      case 'y':
        soundData = simon_yellow;
        soundLength = SIMON_YELLOW_SAMPLES;
        soundLabel = F("Simon yellow");
        break;
      case 't':
        soundData = title;
        soundLength = TITLE_SAMPLES;
        soundLabel = F("Title theme");
        break;
      case 'h':
      case '?':
        printSerialHelp();
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

    if (soundData != nullptr) {
      if (playSound(soundData, soundLength)) {
        logSoundQueued(F("[Serial]"), soundLabel);
      } else {
        Serial.println(F("[Audio] [Serial] Mixer busy, could not queue sound."));
      }
    }
  }
}

void setup() {
  Serial.begin(115200);
  Serial.println();
  Serial.println(F("[Audio] Booting audio module..."));

  Serial.println(F("[Audio] Using MAX98357A I2S amplifier (BCK=GP8, LRCK=GP9, DIN=GP7)"));

  Serial.println(F("[Audio] Initializing CAN bus..."));
  initCanBus(CAN_ID_AUDIO);
  registerCanCallback(onCanMessage);
  Serial.println(F("[Audio] CAN bus ready (ID=C0)"));
  
  // Initialize module_state system (starts in discovery mode)
  initModuleState(MODULE_STATE_NO_LED);  // Audio module doesn't have a status LED
  Serial.println(F("[Audio] Module state initialized"));

  Serial.println(F("[Audio] Waiting for mixer core to initialize..."));
  while (!audioMixerReady()) {
    delay(1);
  }
  Serial.println(F("[Audio] Mixer ready"));

  Serial.println(F("[Audio] Ready. Type 'h' for help."));
}

void loop() {
  handleCanMessages();
  updateModuleState();
  handleSerialInput();
}

void setup1() {
  Serial.println(F("[Audio] Core1 initializing mixer..."));
  initAudioMixer();
  Serial.println(F("[Audio] Core1 mixer setup complete."));
}

void loop1() {
  static bool loggedStart = false;
  if (!loggedStart) {
    Serial.println(F("[Audio] Core1 mixer loop active."));
    loggedStart = true;
  }

  updateAudioMixer();
}