#pragma once

#include "game_state.h"

// Initialize strike display hardware
void initStrikeDisplay();

// Update strike display based on game state
void updateStrikeCount(GameStateManager& gameState);