#include "sd_test.h"

#include <SdFat.h>
#include <SPI.h>
#include "sd_card.h"

// Access the global card and volume from sd_card.cpp
extern SdSpiCard card;
extern FatVolume volume;

namespace {
// Test card and volume objects (separate from main card to avoid conflicts)
SdSpiCard testCard;
FatVolume testVolume;
FatFile testRoot;

const int chipSelect = SD_CS;  // Use the same CS pin as main SD card

bool initializeTestCard() {
  Serial.println(F("\n=== SD Card Test ==="));
  Serial.print(F("Pins configured:"));
  Serial.print(F("  MISO: GPIO "));
  Serial.print(SD_MISO);
  Serial.print(F(", MOSI: GPIO "));
  Serial.print(SD_MOSI);
  Serial.print(F(", SCK: GPIO "));
  Serial.print(SD_SCK);
  Serial.print(F(", CS: GPIO "));
  Serial.println(SD_CS);
  Serial.println();

  // Configure SPI1 pins for SD card
  Serial.println(F("Configuring SPI1..."));
  SPI1.setRX(SD_MISO);  // MISO
  SPI1.setTX(SD_MOSI);  // MOSI
  SPI1.setSCK(SD_SCK);  // SCK
  SPI1.begin();
  delay(100);  // Let SPI stabilize

  // Configure CS pin
  pinMode(chipSelect, OUTPUT);
  digitalWrite(chipSelect, HIGH);
  delay(10);

  Serial.print(F("Initializing SD card..."));
  Serial.flush();

  // Configure SPI for SdFat
  SdSpiConfig config(chipSelect, SHARED_SPI, SD_SCK_MHZ(10), &SPI1);
  
  // Try to initialize the card
  if (!testCard.begin(config)) {
    Serial.println(F("initialization failed. Things to check:"));
    Serial.println(F("* is a card inserted?"));
    Serial.println(F("* Is your wiring correct?"));
    Serial.println(F("* did you change the chipSelect pin to match your shield or module?"));
    Serial.println();
    Serial.println(F("Trying with slower speed..."));
    delay(500);
    
    // Try with slower speed (1MHz)
    SdSpiConfig configSlow(chipSelect, SHARED_SPI, SD_SCK_MHZ(1), &SPI1);
    if (!testCard.begin(configSlow)) {
      Serial.println(F("Still failed with slower speed."));
      Serial.println(F("Please check:"));
      Serial.println(F("  - Card is inserted"));
      Serial.println(F("  - Wiring is correct"));
      Serial.println(F("  - Card is formatted as FAT16/FAT32"));
      Serial.println(F("  - Power is connected (5V for your board)"));
      return false;
    } else {
      Serial.println(F("SUCCESS with slower speed!"));
    }
  } else {
    Serial.println(F("Wiring is correct and a card is present."));
  }

  // Now we will try to open the 'volume'/'partition' - it should be FAT16 or FAT32
  Serial.print(F("\nInitializing FAT volume..."));
  if (!testVolume.init(&testCard)) {
    Serial.println(F("Could not find FAT16/FAT32 partition."));
    Serial.println(F("Make sure you've formatted the card"));
    return false;
  }
  Serial.println(F("SUCCESS"));

  return true;
}

void printCardType() {
  Serial.print(F("\nCard type: "));
  uint8_t cardType = testCard.type();
  switch(cardType) {
    case SD_CARD_TYPE_SD1:
      Serial.println(F("SD1"));
      break;
    case SD_CARD_TYPE_SD2:
      Serial.println(F("SD2"));
      break;
    case SD_CARD_TYPE_SDHC:
      Serial.println(F("SDHC/SDXC"));
      break;
    default:
      Serial.print(F("Unknown ("));
      Serial.print(cardType);
      Serial.println(F(")"));
  }
}

void printVolumeInfo() {
  // print the type and size of the first FAT-type volume
  uint32_t volumesize;
  Serial.print(F("\nVolume type is FAT"));
  Serial.println(testVolume.fatType(), DEC);
  Serial.println();

  volumesize = testVolume.sectorsPerCluster();    // clusters are collections of sectors
  volumesize *= testVolume.clusterCount();       // we'll have a lot of clusters
  volumesize *= 512;                            // SD card sectors are always 512 bytes
  Serial.print(F("Volume size (bytes): "));
  Serial.println(volumesize);
  Serial.print(F("Volume size (Kbytes): "));
  volumesize /= 1024;
  Serial.println(volumesize);
  Serial.print(F("Volume size (Mbytes): "));
  volumesize /= 1024;
  Serial.println(volumesize);
}

}  // namespace

bool runSdCardFullTest() {
  if (!initializeTestCard()) {
    Serial.println(F("\n=== Test FAILED ==="));
    return false;
  }

  printCardType();
  printVolumeInfo();

  Serial.println(F("\nFiles found on the card (name, date and size in bytes): "));
  if (!testRoot.openRoot(&testVolume)) {
    Serial.println(F("Failed to open root directory."));
    Serial.println(F("\n=== Test FAILED ==="));
    return false;
  }

  // list all files in the card with date and size
  testRoot.ls(&Serial, LS_R | LS_DATE | LS_SIZE);
  testRoot.close();
  
  Serial.println(F("\n=== Test Complete ==="));
  return true;
}

bool runSdCardBasicTest() {
  if (!initializeTestCard()) {
    Serial.println(F("\n=== Basic Test FAILED ==="));
    return false;
  }

  printCardType();
  Serial.println(F("\n=== Basic Test PASSED ==="));
  return true;
}

void printSdCardDetails() {
  if (!initializeTestCard()) {
    Serial.println(F("\n=== Failed to initialize card ==="));
    return;
  }

  printCardType();
  printVolumeInfo();
  
  Serial.println(F("\n=== Details Complete ==="));
}

void listSdCardFiles() {
  if (!initializeTestCard()) {
    Serial.println(F("\n=== Failed to initialize card ==="));
    return;
  }

  Serial.println(F("\nFiles found on the card (name, date and size in bytes): "));
  if (!testRoot.openRoot(&testVolume)) {
    Serial.println(F("Failed to open root directory."));
    return;
  }

  // list all files in the card with date and size
  testRoot.ls(&Serial, LS_R | LS_DATE | LS_SIZE);
  testRoot.close();
  
  Serial.println(F("\n=== File Listing Complete ==="));
}

bool testSdCardReadWrite() {
  if (!initializeTestCard()) {
    Serial.println(F("\n=== Read/Write Test FAILED (initialization) ==="));
    return false;
  }

  Serial.println(F("\n=== Read/Write Test ==="));
  
  // Open root directory (required for file operations)
  FatFile root;
  if (!root.openRoot(&testVolume)) {
    Serial.println(F("Failed to open root directory."));
    Serial.println(F("\n=== Read/Write Test FAILED ==="));
    return false;
  }
  
  // Try to open a test file
  FatFile testFile;
  const char* testFileName = "test.txt";  // Relative path (no leading slash)
  
  // Try to open for reading first
  if (testFile.open(&root, testFileName, O_RDONLY)) {
    Serial.print(F("Test file exists: /"));
    Serial.println(testFileName);
    Serial.print(F("File size: "));
    Serial.print(testFile.fileSize());
    Serial.println(F(" bytes"));
    
    // Read first 100 bytes if available
    if (testFile.fileSize() > 0) {
      char buffer[101];
      size_t bytesRead = testFile.read(buffer, min(100, (int)testFile.fileSize()));
      buffer[bytesRead] = '\0';
      Serial.print(F("First "));
      Serial.print(bytesRead);
      Serial.println(F(" bytes:"));
      Serial.println(buffer);
    }
    testFile.close();
    root.close();
    Serial.println(F("\n=== Read Test PASSED ==="));
    return true;
  } else {
    Serial.print(F("Test file does not exist: /"));
    Serial.println(testFileName);
    Serial.println(F("(This is OK - file doesn't need to exist for read test)"));
  }

  // Try to create and write a test file
  Serial.println(F("\nAttempting write test..."));
  if (testFile.open(&root, testFileName, O_WRONLY | O_CREAT | O_TRUNC)) {
    const char* testData = "SD Card Read/Write Test - Success!";
    size_t bytesWritten = testFile.write(testData, strlen(testData));
    testFile.close();
    
    if (bytesWritten == strlen(testData)) {
      Serial.print(F("Successfully wrote "));
      Serial.print(bytesWritten);
      Serial.println(F(" bytes"));
      
      // Verify by reading it back
      if (testFile.open(&root, testFileName, O_RDONLY)) {
        char readBuffer[100];
        size_t bytesRead = testFile.read(readBuffer, sizeof(readBuffer) - 1);
        readBuffer[bytesRead] = '\0';
        testFile.close();
        
        if (strcmp(readBuffer, testData) == 0) {
          Serial.println(F("Read-back verification: PASSED"));
          root.close();
          Serial.println(F("\n=== Read/Write Test PASSED ==="));
          return true;
        } else {
          Serial.println(F("Read-back verification: FAILED (data mismatch)"));
          Serial.print(F("Expected: "));
          Serial.println(testData);
          Serial.print(F("Got: "));
          Serial.println(readBuffer);
        }
      } else {
        Serial.println(F("Failed to open file for read-back verification"));
      }
    } else {
      Serial.print(F("Write failed (incomplete write: "));
      Serial.print(bytesWritten);
      Serial.print(F(" of "));
      Serial.print(strlen(testData));
      Serial.println(F(" bytes)"));
    }
  } else {
    Serial.println(F("Failed to create test file"));
    Serial.println(F("(Note: Some SD cards may be write-protected)"));
  }

  root.close();
  Serial.println(F("\n=== Read/Write Test FAILED ==="));
  return false;
}

