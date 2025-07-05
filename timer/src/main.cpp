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

// Global game state manager (v2)
GameStateManager gameState;

// These are now handled by GameStateManager

// Callback functions for game state events
void onStateChange(GameState oldState, GameState newState) {
	Serial.print("State changed: ");
	Serial.print(static_cast<int>(oldState));
	Serial.print(" -> ");
	Serial.println(static_cast<int>(newState));
	
	// Play audio based on state changes
	switch (newState) {
		case GameState::EXPLODED:
		{
			// Send explosion sound
			uint8_t explosionSound = AUDIO_EXPLODED;
			sendCanMessage(CAN_ID_AUDIO, &explosionSound, 1);
			break;
		}
			
		case GameState::DEFUSED:
		{
			// Send defusal sound
			uint8_t defusalSound = AUDIO_DEFUSED;
			sendCanMessage(CAN_ID_AUDIO, &defusalSound, 1);
			break;
		}
			
		case GameState::VICTORY:
		{
			// Send victory fanfare
			uint8_t fanfareSound = AUDIO_GAME_OVER_FANFARE;
			sendCanMessage(CAN_ID_AUDIO, &fanfareSound, 1);
			break;
		}
	}
	
	// LCD display is now handled by debug interface
}

void onStrikeChange(uint8_t strikes) {
	Serial.print("Strikes: ");
	Serial.print(strikes);
	Serial.print("/");
	Serial.println(gameState.getMaxStrikes());
	
	// Send strike sound when strike is added
	if (strikes > 0) {
		uint8_t strikeSound = AUDIO_STRIKE;
		sendCanMessage(CAN_ID_AUDIO, &strikeSound, 1);
	}
	
	// LCD display is now handled by debug interface
	// Broadcasting is now handled by GameStateManager
}

void onModuleSolved(uint8_t solved, uint8_t total) {
	Serial.print("Module solved! Progress: ");
	Serial.print(solved);
	Serial.print("/");
	Serial.println(total);
	
	// Send correct chime
	uint8_t correctSound = AUDIO_CORRECT_TIME;
	sendCanMessage(CAN_ID_AUDIO, &correctSound, 1);
	
	// LCD display is now handled by debug interface
}

void onTimeUpdate(unsigned long remainingMs) {
	// This callback is called whenever the timer updates
	// We don't need to do anything here as the countdown display handles it
	
	// Optional: Handle critical time warnings
	if (gameState.isEmergencyTime() && remainingMs > 0) {
		static unsigned long lastWarning = 0;
		unsigned long now = millis();
		if (now - lastWarning >= 10000) { // Every 10 seconds in emergency
			lastWarning = now;
			Serial.print("WARNING: Only ");
			Serial.print(remainingMs / 1000);
			Serial.println(" seconds remaining!");
		}
	}
}

// Simple CAN message callback that delegates to GameStateManager
void onTimerCanMessage(uint16_t id, const uint8_t* data, uint8_t len) {
    gameState.handleCanMessage(id, data, len);
}

// Broadcasting functions moved to GameStateManager

void setup()
{
	// Initialize serial communication
	Serial.begin(115200);
	delay(50); // slight delay for entropy
	randomSeed(millis());

	// Initialize I2C buses
	Wire.setSDA(0);
	Wire.setSCL(1);
	Wire.begin();

	Wire1.setSDA(6);
	Wire1.setSCL(7);
	Wire1.begin();

	// Initialize LCD (debug interface will handle display)
	initLcd1602(16, 2, Wire1);

	// Initialize hardware systems
	initCanBus(CAN_ID_TIMER);
	registerCanCallback(onTimerCanMessage);  // Register our CAN callback
	initStrikeDisplay();
	initCountdownDisplay();
	initDebugInterface();
	initModuleTracker(&gameState);

	// Configure game state
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

	// Set up event callbacks
	gameState.setStateChangeCallback(onStateChange);
	gameState.setStrikeChangeCallback(onStrikeChange);
	gameState.setModuleSolvedCallback(onModuleSolved);
	gameState.setTimeUpdateCallback(onTimeUpdate);

	// Initialize game state (includes initialization sequence)
	gameState.initialize();

	// Display startup information
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

	// LCD display is now handled by debug interface
}

void loop()
{
	// Update game state (handles timer, needy modules, game logic, and initialization)
	gameState.tick();

	// Update hardware displays
	updateCountdownDisplay(gameState);
	updateStrikeCount(gameState);

	// Handle user input
	handleSerialCommands(gameState);
	handleCanMessages();
	
	// Update debug interface
	updateDebugInterface(gameState);
}

// Initialization sequence moved to GameStateManager
