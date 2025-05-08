#pragma once

#include <Arduino.h>
#include <game_state.h>

extern GameStateManager gameState;

void handleSerialCommands();
bool isCountdownActive();
void printHelp();
