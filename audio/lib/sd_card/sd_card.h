#pragma once

#include <Arduino.h>
#include <ktane_console.h>

// SD card SPI pin configuration - update if your wiring differs
constexpr uint8_t SD_MISO = 12;  // GPIO12 (SPI RX / POCI)
constexpr uint8_t SD_CS   = 13;  // GPIO13 (SPI CSn)
constexpr uint8_t SD_SCK  = 14;  // GPIO14 (SPI SCK)
constexpr uint8_t SD_MOSI = 15;  // GPIO15 (SPI TX / PICO)

// Initializes the SD card reader on SPI. Returns true on success.
bool initSdCard();

// Returns true once the SD card has been initialized successfully.
bool sdCardReady();

// Dumps a short summary of the detected SD card (size, type, etc.).
void printSdCardInfo(Stream& output = KTANE_CONSOLE_OUT);

// Lists the SD card from the volume root (recursive tree).
void listSdCardRoot(Stream& output = KTANE_CONSOLE_OUT);

// Lists a subdirectory recursively. Path is volume-relative; a leading '/' is optional
// (e.g. "sounds" or "/sounds"). Use "/" for the same result as listSdCardRoot.
void listSdCardPath(const char* path, Stream& output = KTANE_CONSOLE_OUT);

