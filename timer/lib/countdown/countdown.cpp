#include <Arduino.h>

#include <can_bus.h>

#include <Adafruit_GFX.h>
#include <Adafruit_LEDBackpack.h>

#include <strikes.h>
#include <countdown.h>

#define HT16K33_SEG_ADDRESS 0x70

Adafruit_7segment display = Adafruit_7segment();

static unsigned long countdownDuration = 0;
static unsigned long countdownStartMillis = 0;
static unsigned long lastAdjustedMillis = 0;
static float adjustedElapsedMillis = 0;

static bool countdownRunning = false;

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

float getSpeedMultiplier(uint8_t strikeCount)
{
	return 1.0f + 0.25f * strikeCount;
}

void startCountdown(unsigned long durationMillis)
{
	countdownDuration = durationMillis;
	countdownStartMillis = millis();
	lastAdjustedMillis = countdownStartMillis;
	adjustedElapsedMillis = 0;
	countdownRunning = true;
	lastSecondSent = 0;
}

bool isCountdownRunning()
{
	return countdownRunning;
}

unsigned long getCountdownStartTime()
{
	return countdownStartMillis;
}

void updateCountdownDisplay()
{
	if (!countdownRunning)
		return;

	unsigned long now = millis();
	unsigned long delta = now - lastAdjustedMillis;
	lastAdjustedMillis = now;

	float speed = getSpeedMultiplier(getStrikes());
	adjustedElapsedMillis += delta * speed;

	unsigned long remainingMillis = (adjustedElapsedMillis >= countdownDuration)
										? 0
										: countdownDuration - adjustedElapsedMillis;

	if (remainingMillis == 0)
	{
		countdownRunning = false;
	}

	unsigned long seconds = remainingMillis / 1000;
	int mins = seconds / 60;
	int secs = seconds % 60;
	int timeValue = mins * 100 + secs;

	display.print(timeValue);

	unsigned long blinkRate = (getStrikes() >= 2) ? 125 : 500;
	if (now - lastColonToggle >= blinkRate)
	{
		colonVisible = !colonVisible;
		lastColonToggle = now;
	}

	display.drawColon(colonVisible);
	display.writeDisplay();

	if (seconds != lastSecondSent)
	{
		lastSecondSent = seconds;

		uint8_t sound = (getStrikes() == 2) ? AUDIO_BEEP_HIGH : (getStrikes() == 1) ? AUDIO_BEEP_FAST
																					: AUDIO_BEEP_NORMAL;

		sendCanMessage(CAN_ID_AUDIO, &sound, 1);
	}

	static unsigned long lastEmergencyAlarmSent = 0;

	if (remainingMillis < 60000 && (now - lastEmergencyAlarmSent >= 2000))
	{
		lastEmergencyAlarmSent = now;

		uint8_t emergencySound = AUDIO_ALARM_EMERGENCY;
		sendCanMessage(CAN_ID_AUDIO, &emergencySound, 1);
	}
}
