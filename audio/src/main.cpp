#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <audio_mixer.h>
#include <Adafruit_TPA2016.h>

// Include audio headers (ensure these exist and are correctly formatted)
#include <sounds.h>

Adafruit_TPA2016 amp;

// Handle incoming CAN message
void handleAudioMessage(uint16_t id, const uint8_t *data, uint8_t len)
{
  if (id == CAN_ID_AUDIO && len == 1)
  {
    uint8_t messageId = data[0];
    Serial.print("Audio command: 0x");
    Serial.println(messageId, HEX);

    switch (messageId)
    {
      case AUDIO_BEEP_NORMAL:
        playSound(double_beep, double_beep_len / 2);
        break;
      case AUDIO_BEEP_FAST:
        playSound(double_beep_125, double_beep_125_len / 2);
        break;
      case AUDIO_BEEP_HIGH:
        playSound(time_beep_high, time_beep_high_len / 2);
        break;
      case AUDIO_STRIKE:
        playSound(strike, strike_len / 2);
        break;
      case AUDIO_DEFUSED:
        playSound(bomb_defused, bomb_defused_len / 2);
        break;
      case AUDIO_EXPLODED:
        playSound(explosion, explosion_len / 2);
        break;
      case AUDIO_GAME_OVER_FANFARE:
        playSound(game_over_fanfare, game_over_fanfare_len / 2);
        break;
      case AUDIO_ALARM_CLOCK_BEEP:
        playSound(alarm_clock_beep, alarm_clock_beep_len / 2);
        break;
      case AUDIO_ALARM_CLOCK_SNOOZE:
        playSound(alarm_clock_snooze, alarm_clock_snooze_len / 2);
        break;
      case AUDIO_ALARM_EMERGENCY:
        playSound(alarm_emergency, alarm_emergency_len / 2);
        break;
      default:
        Serial.print("Unknown message ID: ");
        Serial.println(messageId, HEX);
        break;
    }
  }
}

void setup()
{
  Serial.begin(115200);
  Wire.setSDA(0);
  Wire.setSCL(1);
  Wire.begin();

  initAudioMixer(15);
  Serial.println("PWM audio ready.");

  if (!amp.begin())
  {
    Serial.println("Could not find TPA2016D2!");
    while (1)
      ;
  }

  amp.enableChannel(true, false);
  amp.setAGCCompression(TPA2016_AGC_OFF);
  amp.setLimitLevelOn();
  amp.setGain(0);
}

void loop()
{
  handleCanMessages();
  updateAudioMixer();
}