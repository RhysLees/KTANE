#pragma once

#include <Arduino.h>
#include "game_state_v2.h"

// Handle serial commands for game control
void handleSerialCommands(GameStateManager& gameState);

// Legacy compatibility function
bool isCountdownActive();

// Print help text
void printHelp();
