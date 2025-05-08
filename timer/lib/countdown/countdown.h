#pragma once

void initCountdownDisplay();
void updateCountdownRaw(const char* str);
void startCountdown(unsigned long durationMillis);
void updateCountdownDisplay();
bool isCountdownRunning();
unsigned long getCountdownStartTime();
