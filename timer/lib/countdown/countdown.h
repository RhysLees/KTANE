#pragma once

#include "game_state.h"

// Initialize countdown display hardware
void initCountdownDisplay();

// Update countdown display with custom string
void updateCountdownRaw(const char *str);

// Update countdown display based on game state
void updateCountdownDisplay(GameStateManager& gameState);