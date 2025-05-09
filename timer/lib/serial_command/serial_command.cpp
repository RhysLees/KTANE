#include <Arduino.h>
#include <countdown.h>
#include <game_state.h>
#include <can_bus.h>

extern GameStateManager gameState;

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
	CMD_SERIAL
};

CommandType parseCommand(const String &input, String &args)
{
	int spaceIndex = input.indexOf(' ');
	String cmd = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
	args = (spaceIndex == -1) ? "" : input.substring(spaceIndex + 1);
	cmd.trim();
	cmd.toUpperCase();

	if (cmd == "START")
		return CMD_START;
	if (cmd == "STOP")
		return CMD_STOP;
	if (cmd == "RESET")
		return CMD_RESET;
	if (cmd == "TIME")
		return CMD_TIME;
	if (cmd == "STRIKE")
		return CMD_STRIKE;
	if (cmd == "INFO")
		return CMD_INFO;
	if (cmd == "HELP" || cmd == "?")
		return CMD_HELP;
	if (cmd == "SERIAL")
		return CMD_SERIAL;

	return CMD_UNKNOWN;
}

void printHelp()
{
	Serial.println("\nAvailable Commands:");
	Serial.println("  START         - Resume or start the countdown");
	Serial.println("  STOP          - Pause the countdown");
	Serial.println("  RESET         - Reset countdown to set time");
	Serial.println("  TIME mm:ss    - Set countdown duration");
	Serial.println("  STRIKE x      - Set strikes (0–2)");
	Serial.println("  INFO          - Show current state");
	Serial.println("  SERIAL [CLEAR|SHOW|CREDIT] - Control serial display");
	Serial.println("  HELP          - Show this help message\n");
}

void handleSerialCommands()
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
		gameState.startTimer();
		gameState.setState(GAME_RUNNING);
		Serial.println("Countdown resumed.");
		break;

	case CMD_STOP:
		gameState.stopTimer();
		gameState.setState(GAME_IDLE);
		Serial.println("Countdown paused.");
		break;

	case CMD_RESET:
		gameState.setTimeLimit(customCountdownMillis);
		gameState.resetTimer();
		gameState.setState(GAME_IDLE);
		Serial.println("Countdown reset.");
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
			uint8_t next = (gameState.getStrikes() + 1) % (gameState.getMaxStrikes() + 1);
			gameState.setStrikes(next);
			Serial.print("Strikes incremented to ");
			Serial.println(next);
		}
		else
		{
			int strikeVal = args.toInt();
			if (strikeVal < 0 || strikeVal > gameState.getMaxStrikes())
			{
				Serial.println("Strike must be between 0 and 2");
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

	case CMD_SERIAL:
	{
		args.trim();
		args.toUpperCase();
		if (args == "CLEAR")
		{
			uint8_t buf[1] = {SERIAL_DISPLAY_CLEAR};
			sendCanMessage(CAN_INSTANCE_ID(0x20, 0), buf, 1);
			Serial.println("Serial display cleared.");
		}
		else if (args == "SHOW")
		{
			char serial[7];
			String serialStr = gameState.getSerial();
			serialStr.toCharArray(serial, 7);
			uint8_t buf[7];
			buf[0] = SERIAL_DISPLAY_SET_SERIAL;
			memcpy(&buf[1], serial, 6);
			sendCanMessage(CAN_INSTANCE_ID(0x20, 0), buf, 7);
			Serial.print("Serial display showing serial number: ");
			Serial.println(serialStr);
		}
		else if (args == "CREDIT")
		{
			uint8_t buf[1] = {SERIAL_DISPLAY_SHOW_CREDIT};
			sendCanMessage(CAN_INSTANCE_ID(0x20, 0), buf, 1);
			Serial.println("Serial display showing credit.");
		}
		else
		{
			Serial.println("Invalid SERIAL command. Use CLEAR, SHOW, or CREDIT.");
		}
		break;
	}

	case CMD_INFO:
		Serial.println("=== GAME INFO ===");
		Serial.print("State: ");
		switch (gameState.getState())
		{
		case GAME_IDLE:
			Serial.println("IDLE");
			break;
		case GAME_RUNNING:
			Serial.println("RUNNING");
			break;
		case GAME_EXPLODED:
			Serial.println("EXPLODED");
			break;
		case GAME_SOLVED:
			Serial.println("SOLVED");
			break;
		default:
			Serial.println("UNKNOWN");
			break;
		}
		Serial.print("Strikes: ");
		Serial.println(gameState.getStrikes());
		Serial.print("Modules: ");
		Serial.print(gameState.getSolvedModules());
		Serial.print(" solved of ");
		Serial.println(gameState.getTotalModules());
		Serial.print("Countdown: ");
		Serial.println(gameState.isTimerRunning() ? "Running" : "Stopped");
		Serial.print("Time Remaining: ");
		Serial.println(gameState.getRemainingMillis() / 1000);
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
	return gameState.isTimerRunning();
}