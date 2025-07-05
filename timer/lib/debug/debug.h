#pragma once

#include "game_state_v2.h"

// Initialize debug interface (LCD and rotary encoder)
void initDebugInterface();

// Update debug interface with game state
void updateDebugInterface(GameStateManager& gameState);

// Force refresh display (useful for immediate updates)
void refreshDebugDisplay(GameStateManager& gameState);
