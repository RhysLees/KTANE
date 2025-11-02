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
	
	// Notify all modules of game state changes
	bool gameRunning = (newState == GameState::RUNNING);
	uint8_t stateMessage[2];
	stateMessage[0] = gameRunning ? TIMER_GAME_START : TIMER_GAME_STOP;
	stateMessage[1] = (uint8_t)newState;
	sendCanMessage(CAN_ID_BROADCAST, stateMessage, 2);
	
	// Update module tracker
	ModuleTracker* tracker = getModuleTracker();
	if (tracker) {
		tracker->setGameRunning(gameRunning);
	}
}

void onStrikeChange(uint8_t strikes) {
	if (strikes > 0) {
		uint8_t strikeSound[1] = {AUDIO_STRIKE};
		sendCanMessage(CAN_ID_AUDIO, strikeSound, 1);
	}
	
	// Broadcast strike update to all modules
	uint8_t strikeMessage[2] = {TIMER_STRIKE_UPDATE, strikes};
	sendCanMessage(CAN_ID_BROADCAST, strikeMessage, 2);
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
	
	// Broadcast time update
	uint8_t timeMessage[5];
	timeMessage[0] = TIMER_TIME_UPDATE;
	memcpy(&timeMessage[1], &remainingMs, 4);
	sendCanMessage(CAN_ID_BROADCAST, timeMessage, 5);
}

void onTimerCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
    gameState.handleCanMessage(id, senderId, data, len);
}

void onRawCanMessageSerial(uint16_t receiverId, uint16_t senderId, const uint8_t* data, uint8_t len, unsigned long timestamp) {
    Serial.printf("Raw CAN message: Receiver ID: 0x%04X, Sender ID: 0x%04X, Length: %d, Timestamp: %lu\n", receiverId, senderId, len, timestamp);
    Serial.print("Data: ");
    for (uint8_t i = 0; i < len; i++) {
        Serial.printf("0x%02X ", data[i]);
    }
    Serial.println();
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
	config.timeLimitMs = 300000;  // 5 minutes default
	config.maxStrikes = 3;
	config.enableStrikeAcceleration = true;
	config.strikeAccelerationFactor = 0.25f;  // 25% faster per strike
	config.enableEmergencyAlarm = true;
	config.emergencyAlarmThreshold = 60000;   // 1 minute warning
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

void printGameInfo() {
	Serial.println("===============================");
	Serial.println("KTANE Game State v2.0 Ready");
	Serial.println("===============================");
	Serial.print("Serial Number: ");
	Serial.println(gameState.getSerialNumber());
	Serial.print("Time Limit: ");
	Serial.print(gameState.getConfig().timeLimitMs / 1000);
	Serial.println(" seconds");
	Serial.print("Max Strikes: ");
	Serial.println(gameState.getConfig().maxStrikes);
	Serial.println("Module discovery active - 1s heartbeats");
	Serial.println("Type HELP for commands");
	Serial.print("Web UI available at: http://");
	Serial.println(getWiFiIP());
	Serial.println("===============================");
}

void setup() {
	Serial.begin(115200);
	
	setupHardware();
	
	initCanBus(CAN_ID_TIMER);
	registerCanCallback(onTimerCanMessage);
	registerRawCanCallback(onRawCanMessage);
	registerRawCanCallback(onRawCanMessageSerial);
	
	initStrikeDisplay();
	initCountdownDisplay();
	initDebugInterface();
	
	// Initialize module tracker BEFORE game state so it can discover modules
	initModuleTracker(&gameState);

	setupGameConfig();
	setupCallbacks();

	gameState.initialize();
	
	initWebServer(&gameState);

	printGameInfo();
}

void loop() {
	gameState.tick();

	updateCountdownDisplay(gameState);
	updateStrikeCount(gameState);

	handleSerialCommands(gameState);
	handleCanMessages();
	updateModuleConnections();
	
	// Update module tracker to check for timeouts and report discovered modules
	ModuleTracker* tracker = getModuleTracker();
	if (tracker) {
		tracker->update();
	}
	
	updateDebugInterface(gameState);
	updateWebServer();
}
