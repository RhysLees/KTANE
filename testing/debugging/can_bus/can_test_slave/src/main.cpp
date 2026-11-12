#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>
#include <can_bus_test_protocol.h>
#include <can_bus_test_log.h>

namespace {

enum class Phase : uint8_t {
  Negotiate,
  Register,
  Idle,
  RxFlood,
  TxBurst,
};

Phase phase = Phase::Negotiate;
unsigned long negStartMs = 0;
unsigned long negReadyMs = 0;
bool registered = false;
bool discoveryAck = false;

uint16_t rxFloodTarget = 0;
uint32_t rxFloodCount = 0;
unsigned long lastHandleMs = 0;
uint16_t worstHandleGapMs = 0;
uint32_t overflowAtFloodStart = 0;
unsigned long rxFloodStartMs = 0;

uint16_t txBurstTarget = 0;
uint16_t txBurstQueued = 0;

constexpr uint32_t kSyntheticWorkIterations = 1200;
constexpr unsigned long kRxFloodGiveUpMs = 12000;

const char* slavePhaseName(Phase p) {
  switch (p) {
    case Phase::Negotiate:
      return "Negotiate";
    case Phase::Register:
      return "Register";
    case Phase::Idle:
      return "Idle";
    case Phase::RxFlood:
      return "RxFlood";
    case Phase::TxBurst:
      return "TxBurst";
  }
  return "?";
}

void printSlaveStatus(unsigned long now) {
  KTANE_CONSOLE_OUT.print(F("[slave "));
  KTANE_CONSOLE_OUT.print(now - negStartMs);
  KTANE_CONSOLE_OUT.print(F(" ms] phase="));
  KTANE_CONSOLE_OUT.print(slavePhaseName(phase));
  KTANE_CONSOLE_OUT.print(F(" can=0x"));
  KTANE_CONSOLE_OUT.print(getCurrentModuleId(), HEX);
  if (phase == Phase::RxFlood) {
    KTANE_CONSOLE_OUT.print(F(" rx="));
    KTANE_CONSOLE_OUT.print(rxFloodCount);
    KTANE_CONSOLE_OUT.print(F("/"));
    KTANE_CONSOLE_OUT.println(rxFloodTarget);
  } else if (phase == Phase::TxBurst) {
    KTANE_CONSOLE_OUT.print(F(" hb_tx="));
    KTANE_CONSOLE_OUT.print(txBurstQueued);
    KTANE_CONSOLE_OUT.print(F("/"));
    KTANE_CONSOLE_OUT.println(txBurstTarget);
  } else {
    KTANE_CONSOLE_OUT.print(F(" inst="));
    KTANE_CONSOLE_OUT.println(getCurrentInstanceId());
  }
  printCanBusCountersLine(F("  counters"));
}

void syntheticCpuLoad() {
  volatile uint32_t acc = 0;
  for (uint32_t i = 0; i < kSyntheticWorkIterations; i++) {
    acc += i * 2654435761u;
  }
  (void)acc;
}

void recordHandleGap() {
  const unsigned long now = millis();
  if (lastHandleMs != 0) {
    const uint16_t gap = (uint16_t)min(now - lastHandleMs, 65535UL);
    if (gap > worstHandleGapMs) {
      worstHandleGapMs = gap;
    }
  }
  lastHandleMs = now;
}

void sendRxFloodReport() {
  CanBusCounters c = getCanBusCounters();
  uint8_t done[7] = {TEST_CMD_RX_FLOOD_DONE};
  done[1] = (uint8_t)(rxFloodCount & 0xFF);
  done[2] = (uint8_t)((rxFloodCount >> 8) & 0xFF);
  done[3] = (uint8_t)((rxFloodCount >> 16) & 0xFF);
  done[4] = (uint8_t)(worstHandleGapMs & 0xFF);
  done[5] = (uint8_t)((worstHandleGapMs >> 8) & 0xFF);
  uint32_t overflowDelta = c.rxHwOverflow - overflowAtFloodStart;
  done[6] = (uint8_t)min(overflowDelta, 255UL);
  sendCanMessage(CAN_ID_TIMER, done, sizeof(done));
  KTANE_CONSOLE_OUT.print("  rx backlog at end ");
  KTANE_CONSOLE_OUT.println(getCanRxBacklog());
  phase = Phase::Idle;
  KTANE_CONSOLE_OUT.print(F("RX flood report sent, counted "));
  KTANE_CONSOLE_OUT.println(rxFloodCount);
  printCanBusCountersLine(F("slave flood end"));
}

void sendStatsReport() {
  CanBusCounters c = getCanBusCounters();
  const uint16_t negMs = negReadyMs > negStartMs ? (uint16_t)(negReadyMs - negStartMs) : 0;
  uint8_t rep[6] = {TEST_CMD_STATS, getCurrentInstanceId(), (uint8_t)(negMs & 0xFF), (uint8_t)((negMs >> 8) & 0xFF),
                    (uint8_t)min(c.rxHwOverflow, 255UL), (uint8_t)min(c.txDropped, 255UL)};
  sendCanMessage(CAN_ID_TIMER, rep, sizeof(rep));
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  if (len < 1) {
    return;
  }
  const uint8_t msg = data[0];

  if (msg == TIMER_MODULE_DISCOVERED) {
    discoveryAck = true;
    KTANE_CONSOLE_OUT.println("TIMER_MODULE_DISCOVERED received");
    return;
  }

  if (phase == Phase::RxFlood && msg == MODULE_TELEMETRY && id == CAN_ID_BROADCAST && len >= 2 &&
      data[1] == MODULE_TELEMETRY_GENERAL) {
    rxFloodCount++;
    if (rxFloodTarget > 0 && rxFloodCount >= rxFloodTarget) {
      sendRxFloodReport();
      KTANE_CONSOLE_OUT.println("RX flood complete");
    }
    return;
  }

  if (id != getCurrentModuleId() && id != CAN_ID_BROADCAST) {
    return;
  }
  (void)senderId;

  if (msg == TEST_CMD_START_RX_FLOOD && len >= 3) {
    rxFloodTarget = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    rxFloodCount = 0;
    worstHandleGapMs = 0;
    lastHandleMs = millis();
    overflowAtFloodStart = getCanBusCounters().rxHwOverflow;
    rxFloodStartMs = millis();
    phase = Phase::RxFlood;
    KTANE_CONSOLE_OUT.print(F("RX flood armed, target="));
    KTANE_CONSOLE_OUT.println(rxFloodTarget);
    printCanBusCountersLine(F("slave flood start"));
    return;
  }

  if (msg == TEST_CMD_START_TX_BURST && len >= 3) {
    txBurstTarget = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
    txBurstQueued = 0;
    phase = Phase::TxBurst;
    KTANE_CONSOLE_OUT.print("TX burst, target=");
    KTANE_CONSOLE_OUT.println(txBurstTarget);
    return;
  }

  if (msg == TEST_CMD_PING) {
    uint8_t pong[6] = {TEST_CMD_PONG};
    for (uint8_t i = 1; i < 6 && i < len; i++) {
      pong[i] = data[i];
    }
    sendCanMessage(CAN_ID_TIMER, pong, 6);
    return;
  }

  if (msg == TEST_CMD_REPORT_STATS) {
    sendStatsReport();
    return;
  }
}

void runTxBurst() {
  if (txBurstQueued < txBurstTarget) {
    uint8_t hb[] = {MODULE_HEARTBEAT};
    if (sendCanMessage(CAN_ID_TIMER, hb, 1)) {
      txBurstQueued++;
    }
    return;
  }
  uint8_t done[3] = {TEST_CMD_TX_BURST_DONE, (uint8_t)(txBurstQueued & 0xFF),
                     (uint8_t)((txBurstQueued >> 8) & 0xFF)};
  sendCanMessage(CAN_ID_TIMER, done, sizeof(done));
  phase = Phase::Idle;
  KTANE_CONSOLE_OUT.println("TX burst complete");
}

}  // namespace

void setup() {
  pinMode(LED_BUILTIN, OUTPUT);
  for (int i = 0; i < 3; i++) {
    digitalWrite(LED_BUILTIN, HIGH);
    delay(100);
    digitalWrite(LED_BUILTIN, LOW);
    delay(100);
  }

  ktaneConsoleInit(115200);
  negStartMs = millis();

  KTANE_CONSOLE_OUT.println(F("CAN test SLAVE (DEBUGGER_SENDER)"));
  initCanBus(CAN_INSTANCE_ID(CAN_TYPE_DEBUGGER_SENDER, 0x00));
  registerCanCallback(onCanMessage);
}

void loop() {
  handleCanMessages();

  static unsigned long serialGraceEndsAt = 0;
  static bool serialGraceDone = false;
  static unsigned long serialGraceLastSec = 0;
  if (!tickSerialGracePeriod(serialGraceEndsAt, serialGraceDone, serialGraceLastSec)) {
    return;
  }

  static bool idAssignStarted = false;
  if (!idAssignStarted) {
    idAssignStarted = true;
    negStartMs = millis();
    assignUniqueId(CAN_TYPE_DEBUGGER_SENDER);
    KTANE_CONSOLE_OUT.println(F("ID negotiation started"));
  }

  const bool underRxStress = (phase == Phase::RxFlood || phase == Phase::TxBurst);
  if (!underRxStress) {
    syntheticCpuLoad();
  }
  recordHandleGap();

  static unsigned long lastAlive = 0;
  const unsigned long nowAlive = millis();
  digitalWrite(LED_BUILTIN, (nowAlive / 500) % 2);

  if (nowAlive - lastAlive >= 3000) {
    lastAlive = nowAlive;
    printSlaveStatus(nowAlive);
    KTANE_CONSOLE_OUT.flush();
  }

  if (phase == Phase::Negotiate && isCanIdReady()) {
    negReadyMs = millis();
    phase = Phase::Register;
    KTANE_CONSOLE_OUT.print("ID ready in ");
    KTANE_CONSOLE_OUT.print(negReadyMs - negStartMs);
    KTANE_CONSOLE_OUT.print(" ms | instance ");
    KTANE_CONSOLE_OUT.print(getCurrentInstanceId());
    KTANE_CONSOLE_OUT.print(" | CAN 0x");
    KTANE_CONSOLE_OUT.println(getCurrentModuleId(), HEX);
  }

  if (phase == Phase::Register || (phase == Phase::Idle && !registered)) {
    static unsigned long lastReg = 0;
    const unsigned long now = millis();
    if (isCanIdReady() && !registered && now - lastReg >= 500) {
      lastReg = now;
      uint8_t reg[] = {MODULE_REGISTER};
      sendCanMessage(CAN_ID_TIMER, reg, 1);
      if (discoveryAck) {
        registered = true;
        phase = Phase::Idle;
      }
    }
  }

  if (phase == Phase::RxFlood && rxFloodTarget > 0 && rxFloodCount < rxFloodTarget &&
      millis() - rxFloodStartMs >= kRxFloodGiveUpMs) {
    sendRxFloodReport();
    KTANE_CONSOLE_OUT.println("RX flood time limit (partial)");
  }

  static uint32_t lastRxMilestone = 0;
  if (phase == Phase::RxFlood && rxFloodCount > 0 && rxFloodCount % 15 == 0 && rxFloodCount != lastRxMilestone) {
    lastRxMilestone = rxFloodCount;
    KTANE_CONSOLE_OUT.print(F("  flood progress rx "));
    KTANE_CONSOLE_OUT.print(rxFloodCount);
    KTANE_CONSOLE_OUT.print(F("/"));
    KTANE_CONSOLE_OUT.println(rxFloodTarget);
  }

  if (phase == Phase::TxBurst) {
    runTxBurst();
  }
}
