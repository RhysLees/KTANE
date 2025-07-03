#include <Arduino.h>
#include <can_bus.h>
#include "serial_command.h"

static unsigned long customCountdownMillis = 5 * 60 * 1000UL;

#define SERIAL_DISPLAY_CLEAR 0x00
#define SERIAL_DISPLAY_SET_SERIAL 0x01
#define SERIAL_DISPLAY_SHOW_CREDIT 0x02

enum CommandType
{
	CMD_UNKNOWN,
	CMD_START,
	CMD_STOP,
	CMD_RESET,
	CMD_TIME,
	CMD_STRIKE,
	CMD_INFO,
	CMD_HELP,
	CMD_SERIAL,
	CMD_MODULE,
	CMD_SOLVE,
	CMD_EDGEWORK,
	CMD_STATUS,
	CMD_CONFIG
};

CommandType parseCommand(const String &input, String &args)
{
	int spaceIndex = input.indexOf(' ');
	String cmd = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
	args = (spaceIndex == -1) ? "" : input.substring(spaceIndex + 1);
	cmd.trim();
	cmd.toUpperCase();

	if (cmd == "START") return CMD_START;
	if (cmd == "STOP") return CMD_STOP;
	if (cmd == "RESET") return CMD_RESET;
	if (cmd == "TIME") return CMD_TIME;
	if (cmd == "STRIKE") return CMD_STRIKE;
	if (cmd == "INFO") return CMD_INFO;
	if (cmd == "HELP" || cmd == "?") return CMD_HELP;
	if (cmd == "SERIAL") return CMD_SERIAL;
	if (cmd == "MODULE") return CMD_MODULE;
	if (cmd == "SOLVE") return CMD_SOLVE;
	if (cmd == "EDGEWORK") return CMD_EDGEWORK;
	if (cmd == "STATUS") return CMD_STATUS;
	if (cmd == "CONFIG") return CMD_CONFIG;

	return CMD_UNKNOWN;
}

void printHelp()
{
	Serial.println("\nKTANE Game State v2.0 Commands:");
	Serial.println("  START           - Start/resume the game");
	Serial.println("  STOP            - Pause the game");
	Serial.println("  RESET           - Reset game to initial state");
	Serial.println("  TIME mm:ss      - Set countdown duration");
	Serial.println("  STRIKE x        - Set strike count (0–3)");
	Serial.println("  MODULE id type  - Register a module");
	Serial.println("  SOLVE id        - Mark module as solved");
	Serial.println("  STATUS          - Show detailed game status");
	Serial.println("  EDGEWORK        - Show edgework information");
	Serial.println("  CONFIG          - Show configuration");
	Serial.println("  SERIAL [cmd]    - Control serial display");
	Serial.println("  INFO            - Show basic game info");
	Serial.println("  HELP            - Show this help message\n");
}

void handleSerialCommands(GameStateManager& gameState)
{
	if (!Serial.available())
		return;

	String input = Serial.readStringUntil('\n');
	input.trim();

	String args;
	CommandType cmdType = parseCommand(input, args);

	switch (cmdType)
	{
	case CMD_START:
		if (gameState.getState() == GameState::PAUSED) {
			gameState.resumeTimer();
		} else {
			gameState.setState(GameState::RUNNING);
			gameState.startTimer();
		}
		Serial.println("Game started/resumed.");
		break;

	case CMD_STOP:
		gameState.pauseTimer();
		Serial.println("Game paused.");
		break;

	case CMD_RESET:
		gameState.reset();
		Serial.println("Game reset.");
		break;

	case CMD_TIME:
	{
		int colonIndex = args.indexOf(':');
		if (colonIndex != -1)
		{
			int mins = args.substring(0, colonIndex).toInt();
			int secs = args.substring(colonIndex + 1).toInt();
			customCountdownMillis = (mins * 60UL + secs) * 1000UL;
			gameState.setTimeLimit(customCountdownMillis);
			gameState.resetTimer();
			Serial.print("Time set to ");
			Serial.println(args);
		}
		else
		{
			Serial.println("Invalid TIME format. Use mm:ss");
		}
		break;
	}

	case CMD_STRIKE:
	{
		args.trim();
		if (args.isEmpty())
		{
			gameState.addStrike();
			Serial.print("Strike added. Total: ");
			Serial.println(gameState.getStrikes());
		}
		else
		{
			int strikeVal = args.toInt();
			if (strikeVal < 0 || strikeVal > gameState.getMaxStrikes())
			{
				Serial.print("Strike must be between 0 and ");
				Serial.println(gameState.getMaxStrikes());
			}
			else
			{
				gameState.setStrikes(strikeVal);
				Serial.print("Strikes set to ");
				Serial.println(strikeVal);
			}
		}
		break;
	}

	case CMD_MODULE:
	{
		// Format: MODULE <id> <type>
		// Example: MODULE 16 10 (register module with CAN ID 16 and type 0x10)
		args.trim();
		int spaceIndex = args.indexOf(' ');
		if (spaceIndex != -1) {
			uint16_t canId = args.substring(0, spaceIndex).toInt();
			uint8_t moduleType = args.substring(spaceIndex + 1).toInt();
			gameState.registerModule(canId, static_cast<ModuleType>(moduleType));
			Serial.print("Module registered: ID=0x");
			Serial.print(canId, HEX);
			Serial.print(" Type=0x");
			Serial.println(moduleType, HEX);
		} else {
			Serial.println("Usage: MODULE <id> <type>");
		}
		break;
	}

	case CMD_SOLVE:
	{
		args.trim();
		if (!args.isEmpty()) {
			uint16_t canId = args.toInt();
			gameState.setModuleSolved(canId);
			Serial.print("Module solved: ID=0x");
			Serial.println(canId, HEX);
		} else {
			Serial.println("Usage: SOLVE <id>");
		}
		break;
	}

	case CMD_STATUS:
		gameState.printStatus();
		gameState.printModules();
		break;

	case CMD_EDGEWORK:
		gameState.printEdgework();
		break;

	case CMD_CONFIG:
	{
		GameConfig config = gameState.getConfig();
		Serial.println("=== CONFIGURATION ===");
		Serial.print("Time Limit: "); Serial.print(config.timeLimitMs / 1000); Serial.println("s");
		Serial.print("Max Strikes: "); Serial.println(config.maxStrikes);
		Serial.print("Strike Acceleration: "); Serial.println(config.enableStrikeAcceleration ? "ON" : "OFF");
		Serial.print("Acceleration Factor: "); Serial.println(config.strikeAccelerationFactor);
		Serial.print("Emergency Alarm: "); Serial.println(config.enableEmergencyAlarm ? "ON" : "OFF");
		Serial.print("Emergency Threshold: "); Serial.print(config.emergencyAlarmThreshold / 1000); Serial.println("s");
		Serial.print("Needy Modules: "); Serial.println(config.enableNeedyModules ? "ON" : "OFF");
		Serial.print("Edgework: "); Serial.println(config.enableEdgework ? "ON" : "OFF");
		break;
	}

	case CMD_SERIAL:
	{
		args.trim();
		args.toUpperCase();
		if (args == "CLEAR")
		{
			uint8_t buf[1] = {SERIAL_DISPLAY_CLEAR};
			sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 1);
			Serial.println("Serial display cleared.");
		}
		else if (args == "REGENERATE")
		{
			gameState.generateSerialNumber();
			Serial.print("New serial number: ");
			Serial.println(gameState.getSerialNumber());
		}
		else if (args == "SHOW")
		{
			gameState.setSerialNumber(gameState.getSerialNumber()); // This will send to display
			Serial.print("Serial display showing: ");
			Serial.println(gameState.getSerialNumber());
		}
		else if (args == "CREDIT")
		{
			uint8_t buf[1] = {SERIAL_DISPLAY_SHOW_CREDIT};
			sendCanMessage(CAN_ID_SERIAL_DISPLAY, buf, 1);
			Serial.println("Serial display showing credit.");
		}
		else
		{
			Serial.println("Usage: SERIAL [CLEAR|REGENERATE|SHOW|CREDIT]");
		}
		break;
	}

	case CMD_INFO:
		Serial.println("=== GAME INFO ===");
		Serial.print("State: ");
		switch (gameState.getState())
		{
		case GameState::IDLE:
			Serial.println("IDLE");
			break;
		case GameState::RUNNING:
			Serial.println("RUNNING");
			break;
		case GameState::PAUSED:
			Serial.println("PAUSED");
			break;
		case GameState::EXPLODED:
			Serial.println("EXPLODED");
			break;
		case GameState::DEFUSED:
			Serial.println("DEFUSED");
			break;
		case GameState::VICTORY:
			Serial.println("VICTORY");
			break;
		default:
			Serial.println("UNKNOWN");
			break;
		}
		Serial.print("Strikes: "); Serial.print(gameState.getStrikes()); 
		Serial.print("/"); Serial.println(gameState.getMaxStrikes());
		Serial.print("Modules: "); Serial.print(gameState.getSolvedModules());
		Serial.print("/"); Serial.println(gameState.getTotalModules());
		Serial.print("Timer: "); Serial.println(gameState.isTimerRunning() ? "Running" : "Stopped");
		Serial.print("Time Remaining: "); Serial.print(gameState.getRemainingTime() / 1000); Serial.println("s");
		Serial.print("Serial Number: "); Serial.println(gameState.getSerialNumber());
		break;

	case CMD_HELP:
		printHelp();
		break;

	default:
		Serial.println("Unknown command. Type HELP for list.");
		break;
	}
}

bool isCountdownActive()
{
	// Legacy function - would need game state reference
	return false;
}