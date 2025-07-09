#pragma once

#include "game_state_v2.h"

// Initialize countdown display hardware
void initCountdownDisplay();

// Update countdown display with custom string
void updateCountdownRaw(const char *str);

// Update countdown display based on game state
void updateCountdownDisplay(GameStateManager& gameState);

// Legacy compatibility functions
void startCountdown(unsigned long durationMillis);
bool isCountdownRunning();
unsigned long getCountdownStartTime();