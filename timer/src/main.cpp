#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <countdown.h>
#include <strikes.h>
#include <serial_command.h>
#include <game_state_v2.h>
#include <debug.h>
#include <lcd1602.h>
#include <module_tracker.h>

GameStateManager gameState;
void onStateChange(GameState oldState, GameState newState) {
	switch (newState) {
		case GameState::EXPLODED:
		{
			uint8_t explosionSound[1] = {AUDIO_EXPLODED};
			sendCanMessage(CAN_ID_AUDIO, explosionSound, 1);
			break;
		}
			
		case GameState::DEFUSED:
		{
			uint8_t defusalSound[1] = {AUDIO_DEFUSED};
			sendCanMessage(CAN_ID_AUDIO, defusalSound, 1);
			break;
		}
			
		case GameState::VICTORY:
		{
			uint8_t fanfareSound[1] = {AUDIO_GAME_OVER_FANFARE};
			sendCanMessage(CAN_ID_AUDIO, fanfareSound, 1);
			break;
		}
	}
}

void onStrikeChange(uint8_t strikes) {
	if (strikes > 0) {
		uint8_t strikeSound[1] = {AUDIO_STRIKE};
		sendCanMessage(CAN_ID_AUDIO, strikeSound, 1);
	}
}

void onModuleSolved(uint8_t solved, uint8_t total) {
	uint8_t correctSound[1] = {AUDIO_CORRECT_TIME};
	sendCanMessage(CAN_ID_AUDIO, correctSound, 1);
}

void onTimeUpdate(unsigned long remainingMs) {
	if (gameState.isEmergencyTime() && remainingMs > 0) {
		static unsigned long lastWarning = 0;
		unsigned long now = millis();
		if (now - lastWarning >= 10000) { // Every 10 seconds in emergency
			lastWarning = now;
		}
	}
}

void onTimerCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    gameState.handleCanMessage(id, data, len);
}

void setup()
{
	Serial.begin(115200);
	delay(50); // slight delay for entropy
	randomSeed(millis());

	Wire.setSDA(0);
	Wire.setSCL(1);
	Wire.begin();

	Wire1.setSDA(6);
	Wire1.setSCL(7);
	Wire1.begin();

	initLcd1602(16, 2, Wire1);

	Serial.print("DEBUG: CAN_ID_TIMER = 0x");
	Serial.println(CAN_ID_TIMER, HEX);
	Serial.print("DEBUG: CAN_TYPE_TIMER = 0x");
	Serial.println(CAN_TYPE_TIMER, HEX);
	
	initCanBus(CAN_ID_TIMER);
	registerCanCallback(onTimerCanMessage);
	initStrikeDisplay();
	initCountdownDisplay();
	initDebugInterface();
	initModuleTracker(&gameState);

	delay(10000);

	GameConfig config;
	config.timeLimitMs = 300000;  // 5 minutes default
	config.maxStrikes = 3;
	config.enableStrikeAcceleration = true;
	config.strikeAccelerationFactor = 0.25f;  // 25% faster per strike
	config.enableEmergencyAlarm = true;
	config.emergencyAlarmThreshold = 60000;   // 1 minute warning
	config.enableNeedyModules = true;
	config.enableEdgework = true;

	gameState.setConfig(config);

	gameState.setStateChangeCallback(onStateChange);
	gameState.setStrikeChangeCallback(onStrikeChange);
	gameState.setModuleSolvedCallback(onModuleSolved);
	gameState.setTimeUpdateCallback(onTimeUpdate);

	gameState.initialize(); // includes initialization sequence

	Serial.println("===============================");
	Serial.println("KTANE Game State v2.0 Ready");
	Serial.println("===============================");
	Serial.print("Serial Number: ");
	Serial.println(gameState.getSerialNumber());
	Serial.print("Time Limit: ");
	Serial.print(config.timeLimitMs / 1000);
	Serial.println(" seconds");
	Serial.print("Max Strikes: ");
	Serial.println(config.maxStrikes);
	Serial.println("Type HELP for commands");
	Serial.println("===============================");
}

void loop()
{
	gameState.tick(); // handles timer, needy modules, game logic, and initialization

	updateCountdownDisplay(gameState);
	updateStrikeCount(gameState);

	handleSerialCommands(gameState);
	handleCanMessages();
	updateModuleConnections(); // Check for module timeouts
	
	updateDebugInterface(gameState);
}
