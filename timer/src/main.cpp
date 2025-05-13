#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <countdown.h>
#include <strikes.h>
#include <serial_command.h>
#include <game_state.h>
#include <debug.h>
#include <lcd1602.h>

// Global game state
GameStateManager gameState;

void setup()
{
	Wire.setSDA(0);
	Wire.setSCL(1);
	Wire.begin();

	Wire1.setSDA(6);
	Wire1.setSCL(7);
	Wire1.begin();

	Serial.begin(115200);

	delay(50); // slight delay for entropy
	randomSeed(millis());
	gameState.generateSerial();

	initLcd1602(16, 2, Wire1);
	lcd1602SetColor(LCD_COLOR_GREEN);
	lcd1602PrintLine(0, "KTANE LCD OK");
	lcd1602PrintLine(1, "READY");

	delay(3000);

	initCanBus(CAN_ID_TIMER);
	initStrikeDisplay();
	initCountdownDisplay();
	initDebugInterface(); // 👈 init LCD and rotary encoder

	Serial.print("Generated Serial Number: ");
	Serial.println(gameState.getSerial());

	gameState.setStrikes(0);
	gameState.setMaxStrikes(3);
	gameState.setState(GAME_IDLE);
}

void loop()
{
	gameState.tick();
	updateCountdownDisplay();
	updateStrikeCount();
	handleSerialCommands();
	handleCanMessages();
	updateDebugInterface(); // <- rotary + LCD logic
}
