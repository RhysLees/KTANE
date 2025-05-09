#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <countdown.h>
#include <strikes.h>
#include <serial_command.h>
#include <game_state.h>

// Global game state
GameStateManager gameState;
String serialNumber;

String generateSerialNumber()
{
	const char letters[] = "ABCDEFGHJKLMNPQRSTUVWXZ"; // Excludes 'O' and 'Y'
	const char digits[] = "0123456789";
	char serial[7];
	bool hasLetter = false;
	bool hasDigit = false;

	for (int i = 0; i < 5; ++i)
	{
		if (random(2) == 0)
		{
			serial[i] = letters[random(sizeof(letters) - 1)];
			hasLetter = true;
		}
		else
		{
			serial[i] = digits[random(10)];
			hasDigit = true;
		}
	}

	if (!hasLetter)
		serial[random(5)] = letters[random(sizeof(letters) - 1)];
	if (!hasDigit)
		serial[random(5)] = digits[random(10)];

	serial[5] = digits[random(10)]; // Last char always digit
	serial[6] = '\0';

	return String(serial);
}

void sendSerialToDisplay(const char *serial)
{
	uint8_t buf[7];
	buf[0] = SERIAL_DISPLAY_SET_SERIAL;
	memcpy(&buf[1], serial, 6);
	sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 7);
}

void setup()
{
	Wire.setSDA(0);
	Wire.setSCL(1);
	Wire.begin();

	Serial.begin(115200);

	initCanBus(CAN_ID_TIMER); // Register this as the timer module

	initStrikeDisplay();
	initCountdownDisplay();

	delay(5000); // Allow other modules to boot

	String newSerial = generateSerialNumber();
	gameState.setSerial(newSerial);
	sendSerialToDisplay(newSerial.c_str());
	Serial.print("Generated Serial Number: ");
	Serial.println(newSerial);

	gameState.setStrikes(0);
	gameState.setMaxStrikes(3);
	gameState.setState(GAME_IDLE);
	gameState.setTotalModules(0);
	gameState.setSolvedModules(0);
}

void loop()
{
	updateCountdownDisplay();
	updateStrikeCount();
	handleSerialCommands();
	handleCanMessages();
}
