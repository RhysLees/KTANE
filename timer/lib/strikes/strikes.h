#pragma once

#include "game_state_v2.h"

// Initialize strike display hardware
void initStrikeDisplay();

// Update strike display based on game state
void updateStrikeCount(GameStateManager& gameState);