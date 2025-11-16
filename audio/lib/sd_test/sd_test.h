#ifndef SD_TEST_H
#define SD_TEST_H

#include <Arduino.h>

// Run a full SD card test (initialization, card info, volume info, file listing)
// Returns true if all tests pass
bool runSdCardFullTest();

// Run a basic SD card initialization test
// Returns true if card initializes successfully
bool runSdCardBasicTest();

// Print detailed SD card information (card type, volume info)
void printSdCardDetails();

// List all files on the SD card
void listSdCardFiles();

// Test SD card read/write operations
bool testSdCardReadWrite();

#endif // SD_TEST_H

