#include <Arduino.h>
#include <ktane_console.h>
#include <can_bus.h>
#include <can_bus_test_protocol.h>
#include <can_bus_test_log.h>

#ifndef CAN_TEST_EXPECT_INSTANCE
#define CAN_TEST_EXPECT_INSTANCE 2
#endif
#ifndef CAN_TEST_BLOCK_INSTANCE_1
#define CAN_TEST_BLOCK_INSTANCE_1 1
#endif

namespace {

enum class Phase : uint8_t {
  WaitSlave,
  RunRxFlood,
  RunTxBurst,
  RunPing,
  RunStats,
  Done,
};

Phase phase = Phase::WaitSlave;
uint16_t slaveCanId = 0;
unsigned long phaseDeadline = 0;
unsigned long testStartMs = 0;

uint32_t negProbeCount = 0;
uint32_t negTakenCount = 0;
uint32_t negSynthTaken = 0;
uint8_t slaveInstance = 0;
uint16_t slaveNegMs = 0;
uint32_t heartbeatsSeen = 0;
uint32_t rxFloodSent = 0;
uint32_t rxFloodReported = 0;
bool rxFloodDoneReceived = false;
unsigned long pingSentMs = 0;
bool pongReceived = false;
unsigned long pongRttMs = 0;
bool statsReceived = false;
uint16_t rxFloodWorstGapMs = 0;
uint8_t rxFloodOverflowDelta = 0;
uint16_t txBurstQueuedReport = 0;
uint8_t slaveStatsOverflow = 0;
uint8_t slaveStatsTxDropped = 0;

constexpr unsigned long kWaitSlaveMs = 15000;
constexpr uint16_t kRxFloodCount = 60;
constexpr uint16_t kTxBurstExpect = 40;
constexpr uint16_t kFloodInterMsgUs = 1500;
constexpr unsigned long kRxFloodWaitMs = 20000;

const char* masterPhaseName(Phase p) {
  switch (p) {
    case Phase::WaitSlave:
      return "WaitSlave";
    case Phase::RunRxFlood:
      return "RunRxFlood";
    case Phase::RunTxBurst:
      return "RunTxBurst";
    case Phase::RunPing:
      return "RunPing";
    case Phase::RunStats:
      return "RunStats";
    case Phase::Done:
      return "Done";
  }
  return "?";
}

void printConfigBanner() {
  KTANE_CONSOLE_OUT.println(F("--- test config ---"));
  KTANE_CONSOLE_OUT.print(F("  RX flood frames: "));
  KTANE_CONSOLE_OUT.println(kRxFloodCount);
  KTANE_CONSOLE_OUT.print(F("  flood interval us: "));
  KTANE_CONSOLE_OUT.println(kFloodInterMsgUs);
  KTANE_CONSOLE_OUT.print(F("  TX burst heartbeats: "));
  KTANE_CONSOLE_OUT.println(kTxBurstExpect);
  KTANE_CONSOLE_OUT.print(F("  expect slave instance: "));
  KTANE_CONSOLE_OUT.println(CAN_TEST_EXPECT_INSTANCE);
#if CAN_TEST_BLOCK_INSTANCE_1
  KTANE_CONSOLE_OUT.println(F("  synth ID_TAKEN on inst 1: yes"));
#else
  KTANE_CONSOLE_OUT.println(F("  synth ID_TAKEN on inst 1: no"));
#endif
  KTANE_CONSOLE_OUT.print(F("  master CAN id: 0x"));
  KTANE_CONSOLE_OUT.println(CAN_ID_TIMER, HEX);
  KTANE_CONSOLE_OUT.print(F("  serial grace: "));
  KTANE_CONSOLE_OUT.print(CAN_TEST_SERIAL_GRACE_MS / 1000);
  KTANE_CONSOLE_OUT.println(F(" s after boot"));
  KTANE_CONSOLE_OUT.println(F("-------------------"));
}

void printMasterStatus(unsigned long now) {
  KTANE_CONSOLE_OUT.print(F("[master "));
  KTANE_CONSOLE_OUT.print(now - testStartMs);
  KTANE_CONSOLE_OUT.print(F(" ms] phase="));
  KTANE_CONSOLE_OUT.print(masterPhaseName(phase));
  if (phase != Phase::Done && phase != Phase::WaitSlave) {
    const long left = (long)(phaseDeadline - now);
    KTANE_CONSOLE_OUT.print(F(" deadline_in="));
    KTANE_CONSOLE_OUT.print(left > 0 ? left : 0);
    KTANE_CONSOLE_OUT.print(F("ms"));
  }
  if (slaveCanId != 0) {
    KTANE_CONSOLE_OUT.print(F(" slave=0x"));
    KTANE_CONSOLE_OUT.print(slaveCanId, HEX);
  }
  if (phase == Phase::RunRxFlood) {
    KTANE_CONSOLE_OUT.print(F(" flood_tx="));
    KTANE_CONSOLE_OUT.print(rxFloodSent);
    KTANE_CONSOLE_OUT.print(F("/"));
    KTANE_CONSOLE_OUT.print(kRxFloodCount);
    KTANE_CONSOLE_OUT.print(F(" flood_rx_rpt="));
    KTANE_CONSOLE_OUT.print(rxFloodReported);
    KTANE_CONSOLE_OUT.print(rxFloodDoneReceived ? F(" done") : F(" pending"));
  }
  if (phase == Phase::RunTxBurst) {
    KTANE_CONSOLE_OUT.print(F(" hb_seen="));
    KTANE_CONSOLE_OUT.print(heartbeatsSeen);
    KTANE_CONSOLE_OUT.print(F("/"));
    KTANE_CONSOLE_OUT.println(kTxBurstExpect);
  } else if (phase == Phase::RunPing) {
    KTANE_CONSOLE_OUT.print(F(" ping_sent="));
    KTANE_CONSOLE_OUT.print(pingSentMs);
    KTANE_CONSOLE_OUT.println(F("ms waiting pong"));
  } else if (phase == Phase::RunStats) {
    KTANE_CONSOLE_OUT.println(F(" waiting TEST_CMD_STATS"));
  } else {
    KTANE_CONSOLE_OUT.println();
  }
  if (phase == Phase::WaitSlave || phase == Phase::RunRxFlood || phase == Phase::RunTxBurst) {
    printCanBusCountersLine(F("  counters"));
  }
}

void printSummary();

void onRawNegotiation(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len,
                      unsigned long timestamp) {
  (void)senderId;
  (void)timestamp;
  if (len != 3 || (id & 0x1F) != 0) {
    return;
  }
  if (data[1] != CAN_BUS_TEST_SLAVE_TYPE) {
    return;
  }
  if (data[0] == ID_PROBE) {
    negProbeCount++;
    KTANE_CONSOLE_OUT.print("[neg] PROBE inst=");
    KTANE_CONSOLE_OUT.println(data[2]);
#if CAN_TEST_BLOCK_INSTANCE_1
    if (data[2] == 0x01) {
      uint8_t taken[3] = {ID_TAKEN, CAN_BUS_TEST_SLAVE_TYPE, 0x01};
      if (sendIdNegotiationFrame(CAN_BUS_TEST_SLAVE_TYPE, taken)) {
        negSynthTaken++;
      }
    }
#endif
  } else if (data[0] == ID_TAKEN) {
    negTakenCount++;
    KTANE_CONSOLE_OUT.print("[neg] TAKEN inst=");
    KTANE_CONSOLE_OUT.println(data[2]);
  }
}

bool testArmed = false;

void discoverSlave(uint16_t senderId) {
  if (!testArmed || slaveCanId != 0) {
    return;
  }
  slaveCanId = senderId;
  slaveInstance = senderId & 0x1F;
  unsigned long elapsed = millis() - testStartMs;
  KTANE_CONSOLE_OUT.println(F("------------------------------------------"));
  printCanBusCountersLine(F("master at register"));
  KTANE_CONSOLE_OUT.print("Slave registered at CAN id 0x");
  KTANE_CONSOLE_OUT.println(slaveCanId, HEX);
  KTANE_CONSOLE_OUT.print("Negotiation observed: probes=");
  KTANE_CONSOLE_OUT.print(negProbeCount);
  KTANE_CONSOLE_OUT.print(" taken=");
  KTANE_CONSOLE_OUT.print(negTakenCount);
  KTANE_CONSOLE_OUT.print(" synth_taken=");
  KTANE_CONSOLE_OUT.println(negSynthTaken);
  KTANE_CONSOLE_OUT.print("Slave instance: ");
  KTANE_CONSOLE_OUT.print(slaveInstance);
  KTANE_CONSOLE_OUT.print(" (expect ");
  KTANE_CONSOLE_OUT.print(CAN_TEST_EXPECT_INSTANCE);
  KTANE_CONSOLE_OUT.println(")");
  KTANE_CONSOLE_OUT.print("Time from master start: ");
  KTANE_CONSOLE_OUT.print(elapsed);
  KTANE_CONSOLE_OUT.println(" ms");
  KTANE_CONSOLE_OUT.println("------------------------------------------");

  uint8_t ack[] = {TIMER_MODULE_DISCOVERED};
  sendCanMessage(slaveCanId, ack, 1);
  uint8_t arm[] = {TEST_CMD_START_RX_FLOOD, (uint8_t)(kRxFloodCount & 0xFF),
                   (uint8_t)((kRxFloodCount >> 8) & 0xFF)};
  sendCanMessage(slaveCanId, arm, sizeof(arm));
  rxFloodSent = 0;
  phase = Phase::RunRxFlood;
  phaseDeadline = millis() + kRxFloodWaitMs;
  KTANE_CONSOLE_OUT.print(F(">> RX flood started, deadline "));
  KTANE_CONSOLE_OUT.print(kRxFloodWaitMs);
  KTANE_CONSOLE_OUT.println(F(" ms"));
}

void onCanMessage(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  if (len < 1) {
    return;
  }
  const uint8_t msg = data[0];

  if (msg == MODULE_REGISTER && id == CAN_ID_TIMER) {
    discoverSlave(senderId);
    return;
  }

  if (senderId != slaveCanId) {
    return;
  }

  if (msg == MODULE_HEARTBEAT && phase == Phase::RunTxBurst) {
    heartbeatsSeen++;
    return;
  }

  if (msg == TEST_CMD_RX_FLOOD_DONE && phase == Phase::RunRxFlood) {
    rxFloodDoneReceived = true;
    if (len >= 7) {
      uint32_t received = ((uint32_t)data[1]) | ((uint32_t)data[2] << 8) | ((uint32_t)data[3] << 16);
      rxFloodReported = received;
      uint16_t worstGap = (uint16_t)data[4] | ((uint16_t)data[5] << 8);
      uint8_t overflow = data[6];
      rxFloodWorstGapMs = worstGap;
      rxFloodOverflowDelta = overflow;
      KTANE_CONSOLE_OUT.println("-- RX flood (slave report) --");
      KTANE_CONSOLE_OUT.print("  broadcast telemetry frames received: ");
      KTANE_CONSOLE_OUT.println(received);
      KTANE_CONSOLE_OUT.print("  worst handleCanMessages gap: ");
      KTANE_CONSOLE_OUT.print(worstGap);
      KTANE_CONSOLE_OUT.println(" ms");
      KTANE_CONSOLE_OUT.print("  rxHwOverflow during test: ");
      KTANE_CONSOLE_OUT.println(overflow);
    }
    uint8_t cmd[] = {TEST_CMD_START_TX_BURST, (uint8_t)(kTxBurstExpect & 0xFF),
                     (uint8_t)((kTxBurstExpect >> 8) & 0xFF)};
    sendCanMessage(slaveCanId, cmd, 3);
    phase = Phase::RunTxBurst;
    phaseDeadline = millis() + 6000;
    heartbeatsSeen = 0;
    printCanBusCountersLine(F("master entering TX burst"));
    KTANE_CONSOLE_OUT.println(F(">> TX burst armed (slave should queue heartbeats)"));
    return;
  }

  if (msg == TEST_CMD_TX_BURST_DONE && phase == Phase::RunTxBurst) {
    KTANE_CONSOLE_OUT.println("-- TX burst (slave report) --");
    if (len >= 3) {
      uint16_t queued = (uint16_t)data[1] | ((uint16_t)data[2] << 8);
      txBurstQueuedReport = queued;
      KTANE_CONSOLE_OUT.print("  heartbeats queued: ");
      KTANE_CONSOLE_OUT.println(queued);
    }
    KTANE_CONSOLE_OUT.print("  heartbeats seen at master: ");
    KTANE_CONSOLE_OUT.println(heartbeatsSeen);
    pingSentMs = millis();
    uint8_t ping[] = {TEST_CMD_PING, (uint8_t)(pingSentMs & 0xFF), (uint8_t)((pingSentMs >> 8) & 0xFF),
                      (uint8_t)((pingSentMs >> 16) & 0xFF), (uint8_t)((pingSentMs >> 24) & 0xFF)};
    sendCanMessage(slaveCanId, ping, 5);
    phase = Phase::RunPing;
    phaseDeadline = millis() + 3000;
    return;
  }

  if (msg == TEST_CMD_PONG && phase == Phase::RunPing) {
    pongReceived = true;
    pongRttMs = millis() - pingSentMs;
    KTANE_CONSOLE_OUT.println("-- Ping --");
    KTANE_CONSOLE_OUT.print("  round-trip: ");
    KTANE_CONSOLE_OUT.print(pongRttMs);
    KTANE_CONSOLE_OUT.println(" ms");
    uint8_t stats[] = {TEST_CMD_REPORT_STATS};
    sendCanMessage(slaveCanId, stats, 1);
    phase = Phase::RunStats;
    phaseDeadline = millis() + 3000;
    return;
  }

  if (msg == TEST_CMD_STATS && phase == Phase::RunStats) {
    statsReceived = true;
    KTANE_CONSOLE_OUT.println("-- Final slave counters --");
    if (len >= 6) {
      slaveNegMs = (uint16_t)data[2] | ((uint16_t)data[3] << 8);
      slaveStatsOverflow = data[4];
      slaveStatsTxDropped = data[5];
      KTANE_CONSOLE_OUT.print("  neg time ms=");
      KTANE_CONSOLE_OUT.println(slaveNegMs);
      KTANE_CONSOLE_OUT.print("  instance=");
      KTANE_CONSOLE_OUT.println(data[1]);
      KTANE_CONSOLE_OUT.print("  rxHwOverflow(total)=");
      KTANE_CONSOLE_OUT.println(data[4]);
      KTANE_CONSOLE_OUT.print("  txDropped(total)=");
      KTANE_CONSOLE_OUT.println(data[5]);
    }
    phase = Phase::Done;
    printSummary();
  }
}

void printSummary() {
  const uint16_t rxNeed = (uint16_t)(kRxFloodCount * 9 / 10);
  const uint16_t txNeed = (uint16_t)(kTxBurstExpect / 2);
  const bool negOk = slaveCanId != 0 && negProbeCount >= 3;
  const bool negInstOk = slaveInstance == CAN_TEST_EXPECT_INSTANCE;
#if CAN_TEST_BLOCK_INSTANCE_1
  const bool negConflictOk = negSynthTaken >= 1;
#else
  const bool negConflictOk = true;
#endif
  const bool rxOk = rxFloodDoneReceived && rxFloodReported >= rxNeed;
  const bool txOk = heartbeatsSeen >= txNeed;
  const bool pingOk = pongReceived && pongRttMs < 500;
  const bool statsOk = statsReceived;

  KTANE_CONSOLE_OUT.println();
  KTANE_CONSOLE_OUT.println(F("========== CAN bus test summary =========="));
  KTANE_CONSOLE_OUT.print(F("Elapsed "));
  KTANE_CONSOLE_OUT.print(millis() - testStartMs);
  KTANE_CONSOLE_OUT.println(F(" ms"));
  KTANE_CONSOLE_OUT.println(F("--- configured thresholds ---"));
  KTANE_CONSOLE_OUT.print(F("  neg probes (need >=3): saw "));
  KTANE_CONSOLE_OUT.println(negProbeCount);
  KTANE_CONSOLE_OUT.print(F("  slave instance (need "));
  KTANE_CONSOLE_OUT.print(CAN_TEST_EXPECT_INSTANCE);
  KTANE_CONSOLE_OUT.print(F("): "));
  KTANE_CONSOLE_OUT.println(slaveInstance);
  KTANE_CONSOLE_OUT.print(F("  RX flood slave count (need >= "));
  KTANE_CONSOLE_OUT.print(rxNeed);
  KTANE_CONSOLE_OUT.print(F(" of "));
  KTANE_CONSOLE_OUT.print(kRxFloodCount);
  KTANE_CONSOLE_OUT.print(F("): "));
  KTANE_CONSOLE_OUT.println(rxFloodReported);
  KTANE_CONSOLE_OUT.print(F("  TX burst at master (need >= "));
  KTANE_CONSOLE_OUT.print(txNeed);
  KTANE_CONSOLE_OUT.print(F(" of "));
  KTANE_CONSOLE_OUT.print(kTxBurstExpect);
  KTANE_CONSOLE_OUT.print(F("): "));
  KTANE_CONSOLE_OUT.println(heartbeatsSeen);
  KTANE_CONSOLE_OUT.print(F("  ping RTT (need <500ms): "));
  KTANE_CONSOLE_OUT.print(pongRttMs);
  KTANE_CONSOLE_OUT.println(pongReceived ? F(" ms") : F(" (no pong)"));

  KTANE_CONSOLE_OUT.println(F("--- measured data ---"));
  KTANE_CONSOLE_OUT.print(F("  slave CAN id: 0x"));
  KTANE_CONSOLE_OUT.println(slaveCanId, HEX);
  KTANE_CONSOLE_OUT.print(F("  neg taken frames: "));
  KTANE_CONSOLE_OUT.print(negTakenCount);
  KTANE_CONSOLE_OUT.print(F("  synth ID_TAKEN: "));
  KTANE_CONSOLE_OUT.println(negSynthTaken);
  KTANE_CONSOLE_OUT.print(F("  master flood frames sent: "));
  KTANE_CONSOLE_OUT.println(rxFloodSent);
  KTANE_CONSOLE_OUT.print(F("  slave worst handle gap: "));
  KTANE_CONSOLE_OUT.print(rxFloodWorstGapMs);
  KTANE_CONSOLE_OUT.println(F(" ms"));
  KTANE_CONSOLE_OUT.print(F("  slave RX overflow during flood: "));
  KTANE_CONSOLE_OUT.println(rxFloodOverflowDelta);
  KTANE_CONSOLE_OUT.print(F("  slave hb queued: "));
  KTANE_CONSOLE_OUT.println(txBurstQueuedReport);
  KTANE_CONSOLE_OUT.print(F("  slave neg ms (stats): "));
  KTANE_CONSOLE_OUT.println(slaveNegMs);
  KTANE_CONSOLE_OUT.print(F("  slave total rxHwOverflow (stats): "));
  KTANE_CONSOLE_OUT.println(slaveStatsOverflow);
  KTANE_CONSOLE_OUT.print(F("  slave total txDropped (stats): "));
  KTANE_CONSOLE_OUT.println(slaveStatsTxDropped);

  printCanBusCountersBlock(F("--- master can_bus counters (now) ---"));

  KTANE_CONSOLE_OUT.println(F("--- pass / fail ---"));
  KTANE_CONSOLE_OUT.println(negOk ? F("[PASS] ID negotiation observed") : F("[FAIL] ID negotiation"));
  KTANE_CONSOLE_OUT.println(negInstOk ? F("[PASS] Negotiated instance") : F("[FAIL] Negotiated instance"));
  KTANE_CONSOLE_OUT.println(negConflictOk ? F("[PASS] ID_TAKEN conflict path") : F("[FAIL] ID_TAKEN conflict path"));
  KTANE_CONSOLE_OUT.println(rxOk ? F("[PASS] RX flood delivery") : F("[FAIL] RX flood"));
  KTANE_CONSOLE_OUT.print(txOk ? F("[PASS] TX burst (") : F("[FAIL] TX burst ("));
  KTANE_CONSOLE_OUT.print(heartbeatsSeen);
  KTANE_CONSOLE_OUT.print(F("/"));
  KTANE_CONSOLE_OUT.print(kTxBurstExpect);
  KTANE_CONSOLE_OUT.println(F(")"));
  KTANE_CONSOLE_OUT.println(pingOk ? F("[PASS] Ping round-trip") : F("[FAIL] Ping"));
  KTANE_CONSOLE_OUT.println(statsOk ? F("[PASS] Stats report") : F("[FAIL] Stats report"));
  KTANE_CONSOLE_OUT.println(F("=========================================="));
}

void sendRxFloodTick() {
  if (rxFloodSent >= kRxFloodCount) {
    return;
  }
  static unsigned long lastUs = 0;
  const unsigned long nowUs = micros();
  if (nowUs - lastUs < kFloodInterMsgUs) {
    return;
  }
  lastUs = nowUs;
  uint8_t pkt[3] = {MODULE_TELEMETRY, MODULE_TELEMETRY_GENERAL, (uint8_t)(rxFloodSent & 0xFF)};
  sendCanMessage(CAN_ID_BROADCAST, pkt, 3);
  rxFloodSent++;
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

  KTANE_CONSOLE_OUT.println(F("CAN test MASTER (timer role)"));
  printConfigBanner();
  printCanBusCountersLine(F("master after init"));

  initCanBus(CAN_ID_TIMER);
  registerRawCanCallback(onRawNegotiation);
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
  if (!testArmed) {
    testArmed = true;
    testStartMs = millis();
    phaseDeadline = testStartMs + kWaitSlaveMs;
    KTANE_CONSOLE_OUT.println(F("Waiting for slave MODULE_REGISTER..."));
  }

  static unsigned long lastAlive = 0;
  const unsigned long now = millis();
  digitalWrite(LED_BUILTIN, (now / 500) % 2);

  if (now - lastAlive >= 3000) {
    lastAlive = now;
    printMasterStatus(now);
    KTANE_CONSOLE_OUT.flush();
  }

  static uint32_t lastFloodMilestone = 0;
  if (phase == Phase::RunRxFlood && rxFloodSent > 0 && rxFloodSent % 15 == 0 && rxFloodSent != lastFloodMilestone) {
    lastFloodMilestone = rxFloodSent;
    KTANE_CONSOLE_OUT.print(F("  flood progress sent "));
    KTANE_CONSOLE_OUT.print(rxFloodSent);
    KTANE_CONSOLE_OUT.print(F("/"));
    KTANE_CONSOLE_OUT.println(kRxFloodCount);
  }

  switch (phase) {
    case Phase::WaitSlave:
      if (now > phaseDeadline) {
        KTANE_CONSOLE_OUT.println("Timeout: no slave register.");
        phase = Phase::Done;
        printSummary();
      }
      break;

    case Phase::RunRxFlood:
      sendRxFloodTick();
      if (!rxFloodDoneReceived && now > phaseDeadline && rxFloodSent >= kRxFloodCount) {
        KTANE_CONSOLE_OUT.println("Timeout waiting for RX_FLOOD_DONE");
        phase = Phase::Done;
        printSummary();
      }
      break;

    case Phase::RunTxBurst:
    case Phase::RunPing:
    case Phase::RunStats:
      if (now > phaseDeadline) {
        KTANE_CONSOLE_OUT.print(F("Timeout in phase "));
        KTANE_CONSOLE_OUT.println(masterPhaseName(phase));
        printCanBusCountersLine(F("master at timeout"));
        phase = Phase::Done;
        printSummary();
      }
      break;

    case Phase::Done:
      break;
  }
}
