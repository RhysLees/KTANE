#include "sd_card.h"

#include <SdFat.h>
#include <SPI.h>

// Global SD card objects (matching working sdtest.cpp pattern exactly)
SdSpiCard card;
FatVolume volume;

namespace {
constexpr uint32_t SD_SPI_CLOCK_HZ = 12000000;  // 12 MHz is safe for most cards

bool g_sdReady = false;

const __FlashStringHelper* cardTypeToString(uint8_t type) {
  switch (type) {
    case 0: return F("None");
    case 1: return F("MMC");
    case 2: return F("SDSC v1");
    case 3: return F("SDSC v2");
    case 4: return F("SDHC/SDXC");
    default: return F("Unknown");
  }
}

void printIndent(Stream& output, uint8_t depth) {
  while (depth-- > 0) {
    output.print(F("  "));
  }
}

void listDirectory(FatFile dir, Stream& output, uint8_t depth = 0) {
  if (!dir.isOpen() || !dir.isDir()) {
    output.println(F("[SD] (not a directory)"));
    return;
  }

  // Safety limit to prevent infinite loops
  if (depth > 10) {
    output.println(F("[SD] Directory depth limit reached"));
    return;
  }

  uint16_t fileCount = 0;
  const uint16_t MAX_FILES = 1000;  // Safety limit

  dir.rewind();
  FatFile entry;
  while (entry.openNext(&dir, O_RDONLY) && fileCount < MAX_FILES) {
    printIndent(output, depth);
    char fileName[256];
    entry.getName(fileName, sizeof(fileName));
    output.print(fileName);
    output.flush();  // Ensure output is sent
    
    if (entry.isDir()) {
      output.println(F("/"));
      output.flush();
      listDirectory(entry, output, depth + 1);
    } else {
      output.print(F(" ("));
      output.print(entry.fileSize());
      output.println(F(" bytes)"));
      output.flush();
    }
    entry.close();
    fileCount++;
  }
  
  if (fileCount >= MAX_FILES) {
    output.println(F("[SD] File count limit reached"));
  }
}
}  // namespace

bool initSdCard() {
  // If already initialized, verify it's still working
  if (g_sdReady) {
    FatFile test;
    if (test.open(&volume, "/", O_RDONLY)) {
      test.close();
      return true;  // Still working
    }
    // Card became unavailable - reset state and retry
    Serial.println(F("[SD] Card became unavailable, reinitializing..."));
    g_sdReady = false;
    delay(100);
  }

  Serial.print(F("[SD] Initializing SD card (CS=GP"));
  Serial.print(SD_CS);
  Serial.println(F(", SPI1)..."));
  Serial.print(F("[SD] Pins: MISO=GP"));
  Serial.print(SD_MISO);
  Serial.print(F(", MOSI=GP"));
  Serial.print(SD_MOSI);
  Serial.print(F(", SCK=GP"));
  Serial.println(SD_SCK);

  // Configure SPI1 pins for SD card (exact order from working sdtest.cpp)
  Serial.println(F("[SD] Configuring SPI1..."));
  SPI1.setRX(SD_MISO);  // MISO
  SPI1.setTX(SD_MOSI);  // MOSI
  SPI1.setSCK(SD_SCK);  // SCK
  SPI1.begin();
  delay(100);  // Let SPI stabilize

  // Configure CS pin (exact order from working sdtest.cpp)
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
  delay(10);

  Serial.print(F("[SD] Initializing SD card..."));
  Serial.flush();

  // Configure SPI for SdFat (exact pattern from working sdtest.cpp)
  SdSpiConfig config(SD_CS, SHARED_SPI, SD_SCK_MHZ(10), &SPI1);
  
  // Try to initialize the card (exact pattern from working sdtest.cpp)
  if (!card.begin(config)) {
    Serial.println(F("initialization failed. Things to check:"));
    Serial.println(F("* is a card inserted?"));
    Serial.println(F("* Is your wiring correct?"));
    Serial.println(F("* did you change the chipSelect pin to match your shield or module?"));
    Serial.println();
    Serial.println(F("Trying with slower speed..."));
    delay(500);
    
    // Try with slower speed (1MHz) - exact pattern from working sdtest.cpp
    SdSpiConfig configSlow(SD_CS, SHARED_SPI, SD_SCK_MHZ(1), &SPI1);
    if (!card.begin(configSlow)) {
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
  // (exact pattern from working sdtest.cpp)
  Serial.print(F("\n[SD] Initializing FAT volume..."));
  if (!volume.init(&card)) {
    Serial.println(F("Could not find FAT16/FAT32 partition."));
    Serial.println(F("Make sure you've formatted the card"));
    return false;
  }
  Serial.println(F("SUCCESS"));

  g_sdReady = true;
  Serial.println(F("[SD] Card ready on SPI1."));
  return true;
}

bool sdCardReady() {
  return g_sdReady;
}

void printSdCardInfo(Stream& output) {
  if (!sdCardReady()) {
    output.println(F("[SD] Card not initialized."));
    return;
  }

  // Confirm the card is ready and accessible
  FatFile root;
  if (root.openRoot(&volume)) {
    output.println(F("[SD] Card ready and accessible."));
    root.close();
  } else {
    output.println(F("[SD] Card initialized but not accessible."));
  }
}

void listSdCardRoot(Stream& output) {
  if (!sdCardReady()) {
    output.println(F("[SD] Card not initialized."));
    return;
  }

  FatFile root;
  if (!root.openRoot(&volume)) {
    output.println(F("[SD] Failed to open root directory."));
    return;
  }

  output.println(F("[SD] Root directory:"));
  output.flush();
  listDirectory(root, output);
  root.close();
  output.println(F("[SD] Directory listing complete."));
  output.flush();
}


