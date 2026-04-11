#include <Arduino.h>
#include <audio_mixer.h>
#include <cctype>
#include <cstring>
#include <sd_card.h>

// Canonical WAVs on the host: KTANE_IRL/KTANE_AUDIO (sibling of this repo).
// Copy them to the SD card as sounds/<filename> (e.g. sounds/strike.wav).

namespace {

constexpr size_t kLineBuf = 256;

void trimTrailing(char* s) {
  size_t n = strlen(s);
  while (n > 0 && (s[n - 1] == '\r' || s[n - 1] == '\n' || s[n - 1] == ' ' ||
                   s[n - 1] == '\t')) {
    s[--n] = '\0';
  }
}

void skipSpaces(const char*& p) {
  while (*p == ' ' || *p == '\t') {
    ++p;
  }
}

bool nextToken(const char*& p, char* out, size_t outLen) {
  skipSpaces(p);
  size_t i = 0;
  while (*p && *p != ' ' && *p != '\t' && i + 1 < outLen) {
    out[i++] = *p++;
  }
  out[i] = '\0';
  return i > 0;
}

int icmp(const char* a, const char* b) {
  while (*a && *b) {
    const int ca = tolower(static_cast<unsigned char>(*a));
    const int cb = tolower(static_cast<unsigned char>(*b));
    if (ca != cb) {
      return ca - cb;
    }
    ++a;
    ++b;
  }
  return static_cast<unsigned char>(*a) - static_cast<unsigned char>(*b);
}

void printHelp() {
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println(F("[Console] Commands (lowercase):"));
  KTANE_CONSOLE_OUT.println(F("  h, help     - this help"));
  KTANE_CONSOLE_OUT.println(F("  i           - SD card info"));
  KTANE_CONSOLE_OUT.println(F("  ls          - list full tree from root"));
  KTANE_CONSOLE_OUT.println(F("  ls <path>   - list subtree (e.g. ls sounds or ls /sounds)"));
  KTANE_CONSOLE_OUT.println(
      F("  play <path> - queue WAV (16-bit PCM, mono/stereo, 44100 Hz); up to 6 at once"));
  KTANE_CONSOLE_OUT.println(F("  v <0-100>   - set mixer volume percent"));
  KTANE_CONSOLE_OUT.println(F("  + / -       - volume up/down by 5%"));
  KTANE_CONSOLE_OUT.println();
}

void cmdVolumeDelta(int delta) {
  int v = static_cast<int>(getAudioMixerVolume()) + delta;
  if (v < 0) {
    v = 0;
  }
  if (v > 100) {
    v = 100;
  }
  setAudioMixerVolume(static_cast<uint8_t>(v));
  KTANE_CONSOLE_OUT.print(F("[Console] Volume "));
  KTANE_CONSOLE_OUT.print(getAudioMixerVolume());
  KTANE_CONSOLE_OUT.println(F("%"));
}

void processLine(char* line) {
  trimTrailing(line);
  const char* p = line;
  skipSpaces(p);
  if (*p == '\0') {
    return;
  }

  if (p[0] == '+' && p[1] == '\0') {
    cmdVolumeDelta(5);
    return;
  }
  if (p[0] == '-' && p[1] == '\0') {
    cmdVolumeDelta(-5);
    return;
  }

  char cmd[24];
  if (!nextToken(p, cmd, sizeof cmd)) {
    return;
  }
  skipSpaces(p);
  const char* arg = (*p != '\0') ? p : nullptr;

  if (icmp(cmd, "help") == 0 || icmp(cmd, "h") == 0) {
    printHelp();
    return;
  }
  if (icmp(cmd, "i") == 0) {
    if (!sdCardReady() && !initSdCard()) {
      KTANE_CONSOLE_OUT.println(F("[Console] SD init failed."));
    } else {
      printSdCardInfo(KTANE_CONSOLE_OUT);
    }
    return;
  }
  if (icmp(cmd, "ls") == 0) {
    if (!sdCardReady() && !initSdCard()) {
      KTANE_CONSOLE_OUT.println(F("[Console] SD init failed."));
      return;
    }
    if (arg) {
      listSdCardPath(arg, KTANE_CONSOLE_OUT);
    } else {
      listSdCardRoot(KTANE_CONSOLE_OUT);
    }
    return;
  }
  if (icmp(cmd, "play") == 0) {
    if (arg == nullptr || arg[0] == '\0') {
      KTANE_CONSOLE_OUT.println(F("[Console] Usage: play <path>"));
      return;
    }
    if (!sdCardReady() && !initSdCard()) {
      KTANE_CONSOLE_OUT.println(F("[Console] SD init failed."));
      return;
    }
    KTANE_CONSOLE_OUT.print(F("[Console] Queue play: "));
    KTANE_CONSOLE_OUT.println(arg);
    if (!playSoundFromFile(arg)) {
      KTANE_CONSOLE_OUT.println(F("[Console] Playback failed."));
    }
    return;
  }
  if (icmp(cmd, "v") == 0) {
    if (arg == nullptr) {
      KTANE_CONSOLE_OUT.println(F("[Console] Usage: v <0-100>"));
      return;
    }
    const int v = atoi(arg);
    if (v < 0 || v > 100) {
      KTANE_CONSOLE_OUT.println(F("[Console] Volume must be 0-100."));
      return;
    }
    setAudioMixerVolume(static_cast<uint8_t>(v));
    KTANE_CONSOLE_OUT.print(F("[Console] Volume "));
    KTANE_CONSOLE_OUT.print(getAudioMixerVolume());
    KTANE_CONSOLE_OUT.println(F("%"));
    return;
  }

  KTANE_CONSOLE_OUT.print(F("[Console] Unknown command: "));
  KTANE_CONSOLE_OUT.println(cmd);
}

}  // namespace

void setup() {
  // Picoprobe UART on GP0 (TX) / GP1 (RX), physical pins 1 & 2.
  Serial1.setTX(0);
  Serial1.setRX(1);
  Serial1.begin(115200);
  delay(500);
  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println(F("[Console] Audio SD console (EDGE_AUDIO_CONSOLE)"));
  KTANE_CONSOLE_OUT.println(F("[Console] I2S BCLK=GP3 WSEL=GP4 DIN=GP5; I2C SDA=GP6 SCL=GP7"));

  initAudioMixer();
  while (!audioMixerReady()) {
    delay(1);
  }
  KTANE_CONSOLE_OUT.print(F("[Console] Mixer ready, volume "));
  KTANE_CONSOLE_OUT.print(getAudioMixerVolume());
  KTANE_CONSOLE_OUT.println(F("%"));

  if (initSdCard()) {
    printSdCardInfo(KTANE_CONSOLE_OUT);
  } else {
    KTANE_CONSOLE_OUT.println(F("[Console] SD not ready; use 'i' to retry after insert."));
  }

  printHelp();
  KTANE_CONSOLE_OUT.flush();
}

void loop() {
  updateAudioMixer();

  static char lineBuf[kLineBuf];
  static size_t lineLen = 0;

  while (KTANE_CONSOLE_OUT.available() > 0) {
    const char c = static_cast<char>(KTANE_CONSOLE_OUT.read());
    if (c == '\n' || c == '\r') {
      if (lineLen > 0) {
        lineBuf[lineLen] = '\0';
        processLine(lineBuf);
        lineLen = 0;
      }
    } else if (c >= 32 && c <= 126 && lineLen + 1 < kLineBuf) {
      // Drop control chars and UTF-8/ANSI (e.g. arrow keys send ESC [ A).
      lineBuf[lineLen++] = c;
    }
  }
}

void setup1() {}

// I2S/mixer on core0 with initAudioMixer (same core as i2s.begin); core1 unused.
void loop1() {}
