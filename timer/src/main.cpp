#include <Arduino.h>
#include <Wire.h>
#include <can_bus.h>
#include <countdown.h>
#include <strikes.h>
#include <serial_command.h>
#include <game_state.h>
#include <debug.h>
#include <lcd1602.h>
#include <web_server.h>
#include <web_api.h>

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
	
	bool gameRunning = (newState == GameState::RUNNING);
	uint8_t stateMessage[2];
	stateMessage[0] = gameRunning ? TIMER_GAME_START : TIMER_GAME_STOP;
	stateMessage[1] = (uint8_t)newState;
	sendCanMessage(CAN_ID_BROADCAST, stateMessage, 2);
}

void onStrikeChange(uint8_t strikes) {
	if (strikes > 0) {
		uint8_t strikeSound[1] = {AUDIO_STRIKE};
		sendCanMessage(CAN_ID_AUDIO, strikeSound, 1);
	}
	
	uint8_t strikeMessage[2] = {TIMER_STRIKE_UPDATE, strikes};
	sendCanMessage(CAN_ID_BROADCAST, strikeMessage, 2);
}

void onModuleSolved(uint8_t solved, uint8_t total) {
	uint8_t correctSound[1] = {AUDIO_CORRECT_TIME};
	sendCanMessage(CAN_ID_AUDIO, correctSound, 1);
}

void onTimeUpdate(unsigned long remainingMs) {
	static unsigned long lastTimeUpdateSent = 0;
	unsigned long now = millis();
	
	// Send time update exactly 2 times per second (every 500ms)
	if (now - lastTimeUpdateSent >= 500) {
		uint8_t timeMessage[5];
		timeMessage[0] = TIMER_TIME_UPDATE;
		memcpy(&timeMessage[1], &remainingMs, 4);
		sendCanMessage(CAN_ID_BROADCAST, timeMessage, 5);
		
		lastTimeUpdateSent = now;
	}
	
	if (gameState.isEmergencyTime() && remainingMs > 0) {
		static unsigned long lastWarning = 0;
		if (now - lastWarning >= 10000) {
			lastWarning = now;
		}
	}
}

void onTimerCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
	if (len < 1) {
		return;
	}
	
	// Decode command from first byte and route to appropriate handler
	uint8_t command = data[0];
	
	switch (command) {
		case MODULE_REGISTER:
			gameState.handleCanMessage(id, senderId, data, len);
			break;
		case MODULE_STRIKE:
			gameState.handleCanMessage(id, senderId, data, len);
			break;
		case MODULE_SOLVED:
			gameState.handleCanMessage(id, senderId, data, len);
			break;
		case MODULE_STATUS:
			gameState.handleCanMessage(id, senderId, data, len);
			break;
		case MODULE_HEARTBEAT:
			// Route module messages to game state for processing
			gameState.handleCanMessage(id, senderId, data, len);
			break;
		default:
			// Unknown message type - could add logging or other handlers here
			break;
	}
}

void setupHardware() {
	delay(50);
	randomSeed(millis());

	Wire.setSDA(0);
	Wire.setSCL(1);
	Wire.begin();

	Wire1.setSDA(6);
	Wire1.setSCL(7);
	Wire1.begin();

	initLcd1602(16, 2, Wire1);
}

void setupGameConfig() {
	GameConfig config;
	config.timeLimitMs = 300000;
	config.maxStrikes = 3;
	config.enableStrikeAcceleration = true;
	config.strikeAccelerationFactor = 0.25f;
	config.enableEmergencyAlarm = true;
	config.emergencyAlarmThreshold = 60000;
	config.enableNeedyModules = true;
	config.enableEdgework = true;

	gameState.setConfig(config);
}

void setupCallbacks() {
	gameState.setStateChangeCallback(onStateChange);
	gameState.setStrikeChangeCallback(onStrikeChange);
	gameState.setModuleSolvedCallback(onModuleSolved);
	gameState.setTimeUpdateCallback(onTimeUpdate);
}

void setup() {
	Serial.begin(115200);
	delay(2000);
	
	setupHardware();
	initCanBus(CAN_ID_TIMER);
	registerCanCallback(onTimerCanMessage);
	initStrikeDisplay();
	initCountdownDisplay();
	initDebugInterface();
	setupGameConfig();
	setupCallbacks();
	gameState.initialize();
	initWebServer(&gameState);
}


void loop() {
	gameState.tick();
	updateCountdownDisplay(gameState);
	updateStrikeCount(gameState);
	handleSerialCommands(gameState);
	handleCanMessages();
	
	updateDebugInterface(gameState);
	updateWebServer();
}
