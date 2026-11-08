/**
 * Plays a WAV from the SD card using the same paths as the main audio sound bank
 * (see ../audio/lib/sound_bank/sound_bank.cpp + getSoundFilePath in sound_bank.h).
 *
 * RP2040 SD stutter mitigation:
 * - I2S: more DMA buffers (buffer_count); on Pico buffer_size is setBuffers(..., words), not bytes.
 * - Player: larger StreamCopy buffer (setBufferSize, bytes).
 * - Run player.copy() on core 1 only so core 0 is free for USB/WiFi stack (same idea as EDGE_AUDIO).
 * - Slightly faster SD SPI when the card tolerates it.
 *
 * @see https://github.com/pschatzmann/arduino-audio-tools/discussions/1884
 */

#include <Arduino.h>
#include <SPI.h>
#include <SdFat.h>
#include <can_bus.h>
#include <ktane_console.h>
#include <sound_bank.h>
#include <audio_mixer.h>
#include <sd_card.h>

#include "AudioTools/AudioCodecs/CodecWAV.h"
#include "AudioTools/Disk/AudioSourceSDFAT.h"

namespace {

// Arduino-Pico I2S::setBuffers(nBuffers, bufferWords) — the size is DMA *words* (~int32
// slots per buffer), not bytes. Passing thousands here was ~300KB+ DMA and hung in begin().
// I2S DMA: words per buffer × buffer count (keep total reasonable for 256KB RAM — malloc at begin()).
constexpr int kI2sBufferWords = 1536;
constexpr int kI2sBufferCount = 14;
constexpr int kPlayerCopyBufferBytes = 16384;
// Pump several copy steps per scheduler pass to stay ahead of 44.1k stereo PCM from SD.
constexpr int kCopyBatchesPerLoop1 = 24;

constexpr uint32_t kAudioSampleRate = 44100;
constexpr int kAudioChannels = 2;
constexpr int kAudioBits = 16;

// Which sound-bank entry to play (path string comes from SOUND_ENTRY in sound_bank.cpp).
constexpr uint8_t kTestMessageId = AUDIO_PLAY_TITLE;

// SdFat open() uses paths relative to the volume root (no leading '/').
const char* pathRelativeToVolume(const char* path) {
  if (!path || path[0] == '\0') {
    return nullptr;
  }
  return (path[0] == '/') ? path + 1 : path;
}

}  // namespace

static SdSpiConfig g_sdSpiConfig(
    SD_CS, SHARED_SPI, SD_SCK_MHZ(25), &SPI1);

static AudioSourceSDFAT<> g_source("/", "wav", g_sdSpiConfig, true);
static I2SStream g_i2s;
static WAVDecoder g_decoder;
static AudioPlayer g_player(g_source, g_i2s, g_decoder);

static void setupSdSpi1() {
  SPI1.setRX(SD_MISO);
  SPI1.setTX(SD_MOSI);
  SPI1.setSCK(SD_SCK);
  SPI1.begin();
  delay(50);
  pinMode(SD_CS, OUTPUT);
  digitalWrite(SD_CS, HIGH);
}

void setup() {
  ktaneConsoleInit(115200);
  delay(300);
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println(F("[audio_test] SD WAV -> TLV320 (sound_bank paths + I2S buffers #1884)"));

  const char* bankPath = getSoundFilePath(kTestMessageId);
  const char* sdPath = pathRelativeToVolume(bankPath);
  if (!sdPath) {
    KTANE_CONSOLE_OUT.println(F("[audio_test] getSoundFilePath: unknown message id"));
    while (true) {
      delay(1000);
    }
  }
  KTANE_CONSOLE_OUT.print(F("[audio_test] File: /"));
  KTANE_CONSOLE_OUT.println(sdPath);

  // Same order as ../audio/src/main.cpp: codec + I2S before touching SPI/SD (avoids extra
  // bus activity during sensitive TLV320 I2C programming).
  if (!initAmpPhaseBeforeI2s()) {
    KTANE_CONSOLE_OUT.println(F("[audio_test] TLV320 pre-I2S failed; I2S may still run."));
  }
  KTANE_CONSOLE_OUT.println(F("[audio_test] TLV320 pre-I2S done"));
  KTANE_CONSOLE_OUT.flush();

  AudioInfo info(kAudioSampleRate, kAudioChannels, kAudioBits);
  auto cfg = g_i2s.defaultConfig(TX_MODE);
  cfg.copyFrom(info);
  cfg.pin_bck = AUDIO_MIXER_PIN_BCK;
  cfg.pin_ws = AUDIO_MIXER_PIN_WS;
  cfg.pin_data = AUDIO_MIXER_PIN_DOUT;
  cfg.buffer_size = kI2sBufferWords;
  cfg.buffer_count = kI2sBufferCount;

  KTANE_CONSOLE_OUT.println(F("[audio_test] I2S begin..."));
  KTANE_CONSOLE_OUT.flush();
  if (!g_i2s.begin(cfg)) {
    KTANE_CONSOLE_OUT.println(F("[audio_test] I2S begin failed"));
    while (true) {
      delay(1000);
    }
  }
  delay(50);

  if (!initAmpPhaseAfterI2s()) {
    KTANE_CONSOLE_OUT.println(F("[audio_test] TLV320 post-I2S failed"));
  }
  KTANE_CONSOLE_OUT.println(F("[audio_test] TLV320 + I2S up; init SD SPI..."));
  KTANE_CONSOLE_OUT.flush();

  setupSdSpi1();

  g_source.setAutoNext(false);
  g_player.setBufferSize(kPlayerCopyBufferBytes);
  g_player.setDelayIfOutputFull(1);

  // index -1: mount SD and start decoder chain without picking a track (begin() still returns false).
  KTANE_CONSOLE_OUT.println(F("[audio_test] Player / SD mount..."));
  KTANE_CONSOLE_OUT.flush();
  g_player.begin(-1, false);

  if (!g_player.setPath(sdPath)) {
    KTANE_CONSOLE_OUT.println(F("[audio_test] Open failed — copy WAV to path from sound_bank"));
    while (true) {
      delay(1000);
    }
  }

  KTANE_CONSOLE_OUT.println(F("[audio_test] Playing (copy() on core 1)"));
}

void loop() {
  delay(1);
}

void setup1() {}

void loop1() {
  for (int i = 0; i < kCopyBatchesPerLoop1; ++i) {
    g_player.copy();
  }
}
