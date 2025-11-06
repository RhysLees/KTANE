#pragma once

#include <Arduino.h>
#include "game_state.h"

// Handle serial commands for game control
void handleSerialCommands(GameStateManager& gameState);

// Print help text
void printHelp();
