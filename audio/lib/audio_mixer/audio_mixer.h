#pragma once
#include <Arduino.h>

void initAudioMixer(uint8_t pin = 15);
void updateAudioMixer(); // Call in loop()
void playSound(const int16_t* data, unsigned int length); // length in samples (not bytes)
