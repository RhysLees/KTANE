#include <Arduino.h>

#include <can_bus.h>
#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>
#include "countdown.h"
#include <game_state.h>

extern GameStateManager gameState;

#define HT16K33_SEG_ADDRESS 0x70

Adafruit_7segment display = Adafruit_7segment();

static unsigned long lastColonToggle = 0;
static bool colonVisible = false;
static unsigned long lastSecondSent = 0;

void initCountdownDisplay()
{
	display.begin(HT16K33_SEG_ADDRESS);
	display.clear();
	updateCountdownRaw("----");
}

void updateCountdownRaw(const char *str)
{
	display.clear();
	const uint8_t positions[4] = {0, 1, 3, 4};

	for (int i = 0; i < 4 && str[i] != '\0'; i++)
	{
		uint8_t digitPos = positions[i];
		if (str[i] == '-')
		{
			display.writeDigitRaw(digitPos, 0x40);
		}
		else if (str[i] >= '0' && str[i] <= '9')
		{
			display.writeDigitNum(digitPos, str[i] - '0');
		}
		else
		{
			display.writeDigitRaw(digitPos, 0x00);
		}
	}

	display.writeDisplay();
}

void updateCountdownDisplay(GameStateManager& gameState)
{
	// Only show countdown when game is running
	if (!gameState.isRunning())
	{
		switch (gameState.getState()) {
			case GameState::IDLE:
				updateCountdownRaw("----");
				break;
			case GameState::PAUSED:
				updateCountdownRaw("||");
				break;
			case GameState::EXPLODED:
				updateCountdownRaw("BOOM");
				break;
			case GameState::DEFUSED:
				updateCountdownRaw("SAFE");
				break;
			default:
				updateCountdownRaw("----");
				break;
		}
		return;
	}

	unsigned long now = millis();
	unsigned long remainingMs = gameState.getRemainingTime();

	// Calculate minutes and seconds
	unsigned long seconds = remainingMs / 1000;
	int mins = seconds / 60;
	int secs = seconds % 60;
	int timeValue = mins * 100 + secs;
	
	// Update 7-segment display
	display.print(timeValue);

	// Handle colon blinking - faster when more strikes
	unsigned long blinkRate = (gameState.getStrikes() >= 2) ? 125 : 500;
	if (now - lastColonToggle >= blinkRate)
	{
		colonVisible = !colonVisible;
		lastColonToggle = now;
	}

	display.drawColon(colonVisible);
	display.writeDisplay();

	// Send time beeps based on strike count
	if (seconds != lastSecondSent)
	{
		lastSecondSent = seconds;
		uint8_t sound = (gameState.getStrikes() == 2) ? AUDIO_BEEP_HIGH
						: (gameState.getStrikes() == 1) ? AUDIO_BEEP_FAST
														: AUDIO_BEEP_NORMAL;
		sendCanMessage(CAN_ID_AUDIO, &sound, 1);
	}

	// Send emergency alarm when time is low
	if (gameState.isEmergencyTime())
	{
		static unsigned long lastEmergencyAlarmSent = 0;
		if (now - lastEmergencyAlarmSent >= 3000)
		{
			lastEmergencyAlarmSent = now;
			uint8_t emergencySound = AUDIO_ALARM_EMERGENCY;
			sendCanMessage(CAN_ID_AUDIO, &emergencySound, 1);
		}
	}
}

// Legacy compatibility functions
void startCountdown(unsigned long durationMillis)
{
	// This is now handled by the game state manager
	// Left for backward compatibility
}

bool isCountdownRunning()
{
	// This would need a reference to game state
	// For now, return false for compatibility
	return false;
}

unsigned long getCountdownStartTime()
{
	// Legacy function - not used in new system
	return 0;
}
