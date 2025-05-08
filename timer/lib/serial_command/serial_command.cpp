#include <Arduino.h>
#include <strikes.h>
#include <countdown.h>
#include <game_state.h>

extern GameStateManager gameState;

static unsigned long customCountdownMillis = 5 * 60 * 1000UL;
static bool countdownRunning = false;
static unsigned long countdownOffset = 0;

enum CommandType
{
	CMD_UNKNOWN,
	CMD_START,
	CMD_STOP,
	CMD_RESET,
	CMD_TIME,
	CMD_STRIKE,
	CMD_INFO,
	CMD_HELP
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
		startCountdown(customCountdownMillis - countdownOffset);
		countdownRunning = true;
		Serial.println("Countdown resumed.");
		break;

	case CMD_STOP:
		countdownOffset = millis() - getCountdownStartTime();
		countdownRunning = false;
		Serial.println("Countdown paused.");
		break;

	case CMD_RESET:
		countdownOffset = 0;
		startCountdown(customCountdownMillis);
		countdownRunning = false;
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
			countdownOffset = 0;
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
			setStrikes((getStrikes() + 1) % 3);
			Serial.print("Strikes incremented to ");
			Serial.println(getStrikes());
		}
		else
		{
			int strikeVal = args.toInt();
			if (strikeVal < 0 || strikeVal > 2)
			{
				Serial.println("Strike must be between 0 and 2");
			}
			else
			{
				setStrikes(strikeVal);
				Serial.print("Strikes set to ");
				Serial.println(strikeVal);
			}
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
		Serial.println(countdownRunning ? "Running" : "Stopped");
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
	return countdownRunning;
}
