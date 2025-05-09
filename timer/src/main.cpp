#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <countdown.h>
#include <strikes.h>
#include <serial_command.h>
#include <game_state.h>

// Global game state
GameStateManager gameState;

void setup()
{
	Wire.setSDA(0);
	Wire.setSCL(1);
	Wire.begin();

	Serial.begin(115200);

	delay(50); // slight delay for entropy
	randomSeed(millis());
	gameState.generateSerial();

	initCanBus(CAN_ID_TIMER);
	initStrikeDisplay();
	initCountdownDisplay();

	delay(5000); // Allow other modules to boot

	Serial.print("Generated Serial Number: ");
	Serial.println(gameState.getSerial());

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
