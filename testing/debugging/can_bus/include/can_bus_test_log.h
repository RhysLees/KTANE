#pragma once

#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>

constexpr unsigned long CAN_TEST_SERIAL_GRACE_MS = 10000;

// Call from loop until it returns true; keeps servicing CAN while the host reconnects USB serial.
inline bool tickSerialGracePeriod(unsigned long& graceEndsAt, bool& graceDone, unsigned long& lastCountdownSec) {
  if (graceDone) {
    return true;
  }
  const unsigned long now = millis();
  if (graceEndsAt == 0) {
    graceEndsAt = now + CAN_TEST_SERIAL_GRACE_MS;
    KTANE_CONSOLE_OUT.print(F("Reconnect serial — test starts in "));
    KTANE_CONSOLE_OUT.print(CAN_TEST_SERIAL_GRACE_MS / 1000);
    KTANE_CONSOLE_OUT.println(F(" s"));
    lastCountdownSec = CAN_TEST_SERIAL_GRACE_MS / 1000;
  }
  const unsigned long leftSec = (graceEndsAt > now) ? (graceEndsAt - now + 999) / 1000 : 0;
  if (leftSec != lastCountdownSec) {
    lastCountdownSec = leftSec;
    if (leftSec > 0) {
      KTANE_CONSOLE_OUT.print(F("  ... "));
      KTANE_CONSOLE_OUT.print(leftSec);
      KTANE_CONSOLE_OUT.println(F(" s"));
    }
  }
  if (now < graceEndsAt) {
    return false;
  }
  graceDone = true;
  KTANE_CONSOLE_OUT.println(F(">> Serial grace over — test running"));
  return true;
}

inline void printCanBusCountersLine(const __FlashStringHelper* label) {
  const CanBusCounters c = getCanBusCounters();
  KTANE_CONSOLE_OUT.print(label);
  KTANE_CONSOLE_OUT.print(F(" rxDel="));
  KTANE_CONSOLE_OUT.print(c.rxDelivered);
  KTANE_CONSOLE_OUT.print(F(" rxHwOvf="));
  KTANE_CONSOLE_OUT.print(c.rxHwOverflow);
  KTANE_CONSOLE_OUT.print(F(" rxDrop="));
  KTANE_CONSOLE_OUT.print(c.rxDropped);
  KTANE_CONSOLE_OUT.print(F(" txDone="));
  KTANE_CONSOLE_OUT.print(c.txCompleted);
  KTANE_CONSOLE_OUT.print(F(" txRetry="));
  KTANE_CONSOLE_OUT.print(c.txRetried);
  KTANE_CONSOLE_OUT.print(F(" txDrop="));
  KTANE_CONSOLE_OUT.print(c.txDropped);
  KTANE_CONSOLE_OUT.print(F(" rxQueue="));
  KTANE_CONSOLE_OUT.println(getCanRxBacklog());
}

inline void printCanBusCountersBlock(const __FlashStringHelper* heading) {
  KTANE_CONSOLE_OUT.println(heading);
  printCanBusCountersLine(F("  "));
}
