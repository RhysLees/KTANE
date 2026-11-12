#include <Arduino.h>
#include <SPI.h>
#include <ktane_console.h>
#include <string.h>
#include "mcp_can.h"
#include "can_bus.h"

#define CAN_SPI_PIN 17
#define CAN_INT_PIN 20

// MCP2515 has two RX buffers. Frames we do not care about used to fill both
// and get dropped while Serial printed the last one. Filters keep them out,
// and this service never waits on a TX buffer or on the wire.

static const SPISettings kCanSpi(10000000, MSBFIRST, SPI_MODE0);
static const uint8_t kUserQueueCap = 24;
static const uint8_t kRawQueueCap = 8;
static const uint8_t kRxQueueCap = 32;
static const uint8_t kLogQueueCap = 8;
static const uint8_t kMaxDispatchPerCall = 8;
static const uint8_t kMaxTxAttempts = 3;

volatile bool canInterruptFlag = false;
volatile uint32_t canInterruptCount = 0;
uint16_t thisModuleId = 0xFFFF;
bool canBusInitialized = false;

bool audioModuleConnected = false;
bool serialDisplayConnected = false;
unsigned long lastAudioPing = 0;
unsigned long lastSerialDisplayPing = 0;
#define MODULE_TIMEOUT_MS 5000

bool idConflictDetected = false;
uint8_t currentModuleType = 0;
uint8_t currentInstanceId = 0;

MCP_CAN CAN(CAN_SPI_PIN);

#define MAX_CAN_CALLBACKS 8
static CanMessageCallback canCallbacks[MAX_CAN_CALLBACKS];
static uint8_t callbackCount = 0;

#define MAX_RAW_CAN_CALLBACKS 2
static RawCanMessageCallback rawCanCallbacks[MAX_RAW_CAN_CALLBACKS];
static uint8_t rawCallbackCount = 0;

static bool isrAttached = false;
static bool loggingEnabled = false;
static bool initFailedOnce = false;
static unsigned long nextInitAttempt = 0;
static uint8_t serviceDepth = 0;
static CanBusCounters counters = {};
static unsigned long lastDropLog = 0;

enum class NegPhase : uint8_t {
  Idle,
  Startup,
  SendProbe,
  WaitReply,
  Gap,
  Backoff
};

struct Negotiation {
  NegPhase phase;
  uint8_t moduleType;
  uint8_t candidate;
  uint8_t probesDone;
  bool confirming;
  unsigned long deadline;
};

static Negotiation neg = {NegPhase::Idle, 0, 0, 0, false, 0};

struct UserFrame {
  uint16_t dest;
  uint8_t len;
  uint8_t data[6];
  uint8_t attempts;
  bool high;
};

struct RawFrame {
  uint16_t dest;
  uint8_t len;
  uint8_t data[8];
  uint8_t attempts;
};

struct Inflight {
  bool used;
  bool raw;
  bool high;
  uint8_t attempts;
  uint16_t dest;
  uint8_t len;
  uint8_t data[8];
};

struct RxFrame {
  uint16_t id;
  uint8_t len;
  uint8_t data[8];
  unsigned long timestamp;
};

struct LogFrame {
  bool tx;
  uint16_t id;
  uint16_t senderId;
  uint8_t len;
  uint8_t data[6];
};

static UserFrame userQueue[kUserQueueCap];
static uint8_t userHead = 0;
static uint8_t userCount = 0;

static RawFrame rawQueue[kRawQueueCap];
static uint8_t rawHead = 0;
static uint8_t rawCount = 0;

static Inflight inflight[3];

static RxFrame rxQueue[kRxQueueCap];
static uint8_t rxHead = 0;
static uint8_t rxCount = 0;

static LogFrame logQueue[kLogQueueCap];
static uint8_t logHead = 0;
static uint8_t logCount = 0;

static bool pushFrontUser(const UserFrame& frame);
static bool pushFrontRaw(const RawFrame& frame);

static const uint8_t kLoadTx[3] = {MCP_LOAD_TX0, MCP_LOAD_TX1, MCP_LOAD_TX2};
static const uint8_t kRtsTx[3] = {MCP_RTS_TX0, MCP_RTS_TX1, MCP_RTS_TX2};
static const uint8_t kTxCtrl[3] = {MCP_TXB0CTRL, MCP_TXB1CTRL, MCP_TXB2CTRL};
static const uint8_t kFilterAddr[6] = {
  MCP_RXF0SIDH, MCP_RXF1SIDH, MCP_RXF2SIDH,
  MCP_RXF3SIDH, MCP_RXF4SIDH, MCP_RXF5SIDH
};

void onCanInterrupt() {
  canInterruptFlag = true;
  canInterruptCount++;
}

static void mcpSelect() {
  digitalWrite(CAN_SPI_PIN, LOW);
}

static void mcpUnselect() {
  digitalWrite(CAN_SPI_PIN, HIGH);
}

static void mcpBegin() {
  SPI.beginTransaction(kCanSpi);
  mcpSelect();
}

static void mcpEnd() {
  mcpUnselect();
  SPI.endTransaction();
}

static uint8_t mcpReadReg(uint8_t addr) {
  mcpBegin();
  SPI.transfer(MCP_READ);
  SPI.transfer(addr);
  uint8_t value = SPI.transfer(0x00);
  mcpEnd();
  return value;
}

static void mcpWriteRegs(uint8_t addr, const uint8_t* bytes, uint8_t len) {
  mcpBegin();
  SPI.transfer(MCP_WRITE);
  SPI.transfer(addr);
  for (uint8_t i = 0; i < len; i++) {
    SPI.transfer(bytes[i]);
  }
  mcpEnd();
}

static uint8_t mcpReadStatus() {
  mcpBegin();
  SPI.transfer(MCP_READ_STATUS);
  uint8_t status = SPI.transfer(0x00);
  mcpEnd();
  return status;
}

static void mcpWriteStdFilter(uint8_t addr, uint16_t id11) {
  uint8_t bytes[4];
  bytes[0] = (uint8_t)(id11 >> 3);
  bytes[1] = (uint8_t)((id11 & 0x07) << 5);
  bytes[2] = 0;
  bytes[3] = 0;
  mcpWriteRegs(addr, bytes, 4);
}

static void noteDrop(const char* reason) {
  unsigned long now = millis();
  if ((long)(now - lastDropLog) < 1000) {
    return;
  }
  lastDropLog = now;
  KTANE_CONSOLE_OUT.print("CAN drop: ");
  KTANE_CONSOLE_OUT.println(reason);
}

static void releaseInflightAfterReset() {
  for (uint8_t n = 0; n < 3; n++) {
    if (!inflight[n].used) {
      continue;
    }
    Inflight done = inflight[n];
    inflight[n].used = false;
    if (done.raw) {
      RawFrame retry;
      retry.dest = done.dest;
      retry.len = done.len;
      retry.attempts = done.attempts;
      memcpy(retry.data, done.data, done.len);
      if (!pushFrontRaw(retry)) {
        counters.txDropped++;
      }
    } else {
      UserFrame retry;
      retry.dest = done.dest;
      retry.len = done.len > 6 ? 6 : done.len;
      retry.attempts = done.attempts;
      retry.high = done.high;
      memcpy(retry.data, done.data, retry.len);
      if (!pushFrontUser(retry)) {
        counters.txDropped++;
      }
    }
  }
}

static bool applyAcceptanceFilters() {
  if (CAN.setMode(MODE_CONFIG) != CAN_OK) {
    return false;
  }

  const bool acceptAll = rawCallbackCount > 0;
  if (acceptAll) {
    mcpWriteStdFilter(MCP_RXM0SIDH, 0);
    mcpWriteStdFilter(MCP_RXM1SIDH, 0);
    for (uint8_t i = 0; i < 6; i++) {
      mcpWriteStdFilter(kFilterAddr[i], 0);
    }
  } else {
    const uint16_t typeId = thisModuleId & 0x7E0;
    mcpWriteStdFilter(MCP_RXM0SIDH, 0x7E0);
    mcpWriteStdFilter(kFilterAddr[0], typeId);
    mcpWriteStdFilter(kFilterAddr[1], typeId);
    mcpWriteStdFilter(MCP_RXM1SIDH, 0x7FF);
    for (uint8_t i = 2; i < 6; i++) {
      mcpWriteStdFilter(kFilterAddr[i], CAN_ID_BROADCAST);
    }
  }

  if (CAN.enOneShotTX() != CAN_OK) {
    return false;
  }
  return CAN.setMode(MCP_NORMAL) == CAN_OK;
}

static bool bringUpController() {
  canBusInitialized = false;
  SPI.begin();
  if (CAN.begin(MCP_STDEXT, CAN_500KBPS, MCP_8MHZ) != CAN_OK || !applyAcceptanceFilters()) {
    nextInitAttempt = millis() + 1000;
    if (!initFailedOnce) {
      initFailedOnce = true;
      KTANE_CONSOLE_OUT.println("CAN init failed - retrying");
    }
    return false;
  }

  releaseInflightAfterReset();
  canBusInitialized = true;
  nextInitAttempt = 0;
  if (initFailedOnce) {
    initFailedOnce = false;
    KTANE_CONSOLE_OUT.println("CAN bus recovered");
  }
  return true;
}

static bool due(unsigned long deadline) {
  return (long)(millis() - deadline) >= 0;
}

static bool isHighPriority(uint8_t type) {
  switch (type) {
    case MODULE_STRIKE:
    case MODULE_SOLVED:
    case MODULE_REGISTER:
    case TIMER_GAME_START:
    case TIMER_GAME_STOP:
    case TIMER_STRIKES:
    case TIMER_RESET:
    case TIMER_MODULE_DISCOVERED:
    case TIMER_SERIAL_NUMBER_FIRST_HALF:
    case TIMER_SERIAL_NUMBER_LAST_HALF:
    case AUDIO_PLAY_STRIKE:
    case AUDIO_PLAY_EXPLODED:
    case AUDIO_PLAY_DEFUSED:
      return true;
    default:
      return false;
  }
}

static bool canCoalesce(uint8_t type) {
  return type == TIMER_TIME ||
         type == TIMER_STRIKES ||
         type == TIMER_COUNTDOWN ||
         type == MODULE_HEARTBEAT ||
         type == MODULE_STATUS;
}

static bool dropOldestLowPriority() {
  for (uint8_t i = 0; i < userCount; i++) {
    uint8_t index = (userHead + i) % kUserQueueCap;
    if (userQueue[index].high) {
      continue;
    }
    for (uint8_t j = i; j + 1 < userCount; j++) {
      uint8_t dest = (userHead + j) % kUserQueueCap;
      uint8_t src = (userHead + j + 1) % kUserQueueCap;
      userQueue[dest] = userQueue[src];
    }
    userCount--;
    return true;
  }
  return false;
}

static void enqueueLog(bool tx, uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len) {
  if (!loggingEnabled || logCount >= kLogQueueCap || data == nullptr) {
    return;
  }
  uint8_t index = (logHead + logCount) % kLogQueueCap;
  LogFrame& frame = logQueue[index];
  frame.tx = tx;
  frame.id = id;
  frame.senderId = senderId;
  frame.len = len > 6 ? 6 : len;
  memcpy(frame.data, data, frame.len);
  logCount++;
}

static void printOneLog() {
  if (!loggingEnabled || logCount == 0 || serviceDepth > 0) {
    return;
  }
  LogFrame frame = logQueue[logHead];
  logHead = (logHead + 1) % kLogQueueCap;
  logCount--;
  logCanMessage(frame.tx ? "TX" : "RX", frame.id, frame.senderId, frame.tx ? frame.id : frame.senderId, frame.data, frame.len);
}

static bool enqueueUser(uint16_t dest, const uint8_t* data, uint8_t len) {
  const uint8_t type = data[0];
  const bool high = isHighPriority(type);

  if (canCoalesce(type)) {
    for (uint8_t i = 0; i < userCount; i++) {
      uint8_t index = (userHead + i) % kUserQueueCap;
      UserFrame& existing = userQueue[index];
      if (existing.dest == dest && existing.len > 0 && existing.data[0] == type) {
        existing.len = len;
        existing.high = high;
        memcpy(existing.data, data, len);
        return true;
      }
    }
  }

  if (userCount >= kUserQueueCap) {
    if (!high || !dropOldestLowPriority()) {
      counters.txDropped++;
      noteDrop("TX queue full");
      return false;
    }
  }

  uint8_t index = (userHead + userCount) % kUserQueueCap;
  UserFrame& frame = userQueue[index];
  frame.dest = dest;
  frame.len = len;
  frame.attempts = 0;
  frame.high = high;
  memcpy(frame.data, data, len);
  userCount++;
  return true;
}

static bool enqueueRaw(uint16_t dest, const uint8_t* data, uint8_t len, bool front) {
  if (len > 8 || rawCount >= kRawQueueCap) {
    counters.txDropped++;
    noteDrop("raw TX queue full");
    return false;
  }
  if (front) {
    rawHead = (rawHead + kRawQueueCap - 1) % kRawQueueCap;
    RawFrame& frame = rawQueue[rawHead];
    frame.dest = dest;
    frame.len = len;
    frame.attempts = 0;
    memcpy(frame.data, data, len);
    rawCount++;
    return true;
  }
  uint8_t index = (rawHead + rawCount) % kRawQueueCap;
  RawFrame& frame = rawQueue[index];
  frame.dest = dest;
  frame.len = len;
  frame.attempts = 0;
  memcpy(frame.data, data, len);
  rawCount++;
  return true;
}

static bool pushFrontUser(const UserFrame& frame) {
  if (userCount >= kUserQueueCap) {
    return false;
  }
  userHead = (userHead + kUserQueueCap - 1) % kUserQueueCap;
  userQueue[userHead] = frame;
  userCount++;
  return true;
}

static bool pushFrontRaw(const RawFrame& frame) {
  if (rawCount >= kRawQueueCap) {
    return false;
  }
  rawHead = (rawHead + kRawQueueCap - 1) % kRawQueueCap;
  rawQueue[rawHead] = frame;
  rawCount++;
  return true;
}

static bool mcpLoadAndRequest(uint8_t buffer, uint16_t id, const uint8_t* data, uint8_t len) {
  uint8_t ctrl = mcpReadReg(kTxCtrl[buffer]);
  if (ctrl & MCP_TXB_TXREQ_M) {
    return false;
  }

  mcpBegin();
  SPI.transfer(kLoadTx[buffer]);
  SPI.transfer((uint8_t)(id >> 3));
  SPI.transfer((uint8_t)((id & 0x07) << 5));
  SPI.transfer(0x00);
  SPI.transfer(0x00);
  SPI.transfer(len);
  for (uint8_t i = 0; i < len; i++) {
    SPI.transfer(data[i]);
  }
  mcpUnselect();
  mcpSelect();
  SPI.transfer(kRtsTx[buffer]);
  mcpEnd();
  return true;
}

static void completeTransmissions() {
  for (uint8_t n = 0; n < 3; n++) {
    if (!inflight[n].used) {
      continue;
    }
    uint8_t ctrl = mcpReadReg(kTxCtrl[n]);
    if (ctrl & MCP_TXB_TXREQ_M) {
      continue;
    }

    Inflight done = inflight[n];
    inflight[n].used = false;
    const bool failed = (ctrl & (MCP_TXB_ABTF_M | MCP_TXB_MLOA_M | MCP_TXB_TXERR_M)) != 0;
    if (!failed) {
      counters.txCompleted++;
      continue;
    }

    counters.txRetried++;
    if (done.attempts >= kMaxTxAttempts) {
      counters.txDropped++;
      noteDrop("TX retries exhausted");
      continue;
    }

    if (done.raw) {
      RawFrame retry;
      retry.dest = done.dest;
      retry.len = done.len;
      retry.attempts = done.attempts;
      memcpy(retry.data, done.data, done.len);
      if (!pushFrontRaw(retry)) {
        counters.txDropped++;
        noteDrop("TX retries exhausted");
      }
    } else {
      UserFrame retry;
      retry.dest = done.dest;
      retry.len = done.len > 6 ? 6 : done.len;
      retry.attempts = done.attempts;
      retry.high = done.high;
      memcpy(retry.data, done.data, retry.len);
      if (!pushFrontUser(retry)) {
        counters.txDropped++;
        noteDrop("TX retries exhausted");
      }
    }
  }
}

static int freeTxBuffer() {
  uint8_t status = mcpReadStatus();
  const uint8_t busyBit[3] = {0x04, 0x10, 0x40};
  for (uint8_t n = 0; n < 3; n++) {
    if (!inflight[n].used && (status & busyBit[n]) == 0) {
      return n;
    }
  }
  return -1;
}

static void markInflight(uint8_t buffer, bool raw, bool high, uint8_t attempts, uint16_t dest, const uint8_t* data, uint8_t len) {
  Inflight& slot = inflight[buffer];
  slot.used = true;
  slot.raw = raw;
  slot.high = high;
  slot.attempts = attempts;
  slot.dest = dest;
  slot.len = len;
  memcpy(slot.data, data, len);
}

static void submitTransmissions() {
  while (rawCount > 0) {
    int buffer = freeTxBuffer();
    if (buffer < 0) {
      return;
    }
    RawFrame frame = rawQueue[rawHead];
    if (!mcpLoadAndRequest((uint8_t)buffer, frame.dest, frame.data, frame.len)) {
      return;
    }
    rawHead = (rawHead + 1) % kRawQueueCap;
    rawCount--;
    markInflight((uint8_t)buffer, true, true, (uint8_t)(frame.attempts + 1), frame.dest, frame.data, frame.len);
  }

  if (!isCanIdReady()) {
    return;
  }

  while (userCount > 0) {
    int buffer = freeTxBuffer();
    if (buffer < 0) {
      return;
    }
    UserFrame frame = userQueue[userHead];
    uint8_t wire[8];
    wire[0] = (uint8_t)((thisModuleId >> 8) & 0xFF);
    wire[1] = (uint8_t)(thisModuleId & 0xFF);
    memcpy(&wire[2], frame.data, frame.len);
    const uint8_t wireLen = frame.len + 2;
    if (!mcpLoadAndRequest((uint8_t)buffer, frame.dest, wire, wireLen)) {
      return;
    }
    userHead = (userHead + 1) % kUserQueueCap;
    userCount--;
    markInflight((uint8_t)buffer, false, frame.high, (uint8_t)(frame.attempts + 1), frame.dest, frame.data, frame.len);
    enqueueLog(true, frame.dest, thisModuleId, frame.data, frame.len);
  }
}

static void finishNegotiation(uint8_t instance) {
  currentModuleType = neg.moduleType;
  currentInstanceId = instance;
  updateCanId(CAN_INSTANCE_ID(neg.moduleType, instance));
  neg.phase = NegPhase::Idle;
}

static void failCandidate() {
  uint8_t failed = neg.candidate;
  neg.candidate++;
  neg.probesDone = 0;
  neg.confirming = false;
  idConflictDetected = false;
  if (neg.candidate > ID_MAX_INSTANCE) {
    finishNegotiation(0x01);
    return;
  }
  if (failed >= 3) {
    neg.deadline = millis() + (unsigned long)random(100, 500) * failed;
    neg.phase = NegPhase::Backoff;
  } else {
    neg.phase = NegPhase::SendProbe;
  }
}

static bool stepNegotiation() {
  switch (neg.phase) {
    case NegPhase::Startup:
      if (!due(neg.deadline)) {
        return false;
      }
      neg.phase = NegPhase::SendProbe;
      return true;

    case NegPhase::Gap:
    case NegPhase::Backoff:
      if (idConflictDetected) {
        failCandidate();
        return neg.phase != NegPhase::Idle;
      }
      if (!due(neg.deadline)) {
        return false;
      }
      neg.phase = NegPhase::SendProbe;
      return true;

    case NegPhase::SendProbe: {
      if (idConflictDetected) {
        failCandidate();
        return neg.phase != NegPhase::Idle;
      }
      uint8_t probe[3] = {ID_PROBE, neg.moduleType, neg.candidate};
      if (!enqueueRaw(CAN_INSTANCE_ID(neg.moduleType, 0x00), probe, 3, false)) {
        return false;
      }
      idConflictDetected = false;
      neg.deadline = millis() + 200 + (unsigned long)random(0, 50);
      neg.phase = NegPhase::WaitReply;
      return true;
    }

    case NegPhase::WaitReply:
      if (idConflictDetected) {
        failCandidate();
        return neg.phase != NegPhase::Idle;
      }
      if (!due(neg.deadline)) {
        return false;
      }
      if (neg.confirming) {
        finishNegotiation(neg.candidate);
        return false;
      }
      neg.probesDone++;
      if (neg.probesDone >= 3) {
        neg.confirming = true;
      }
      neg.deadline = millis() + 20 + (unsigned long)random(0, 30);
      neg.phase = NegPhase::Gap;
      return true;

    case NegPhase::Idle:
      break;
  }
  return false;
}

static void serviceNegotiation() {
  if (neg.phase == NegPhase::Idle || !canBusInitialized) {
    return;
  }
  for (uint8_t step = 0; step < 4; step++) {
    if (!stepNegotiation()) {
      return;
    }
  }
}

static bool isNegotiationFrame(uint16_t id, const uint8_t* data, uint8_t len) {
  if (len != 3 || (id & 0x1F) != 0 || id == CAN_ID_BROADCAST) {
    return false;
  }
  if (data[0] != ID_PROBE && data[0] != ID_TAKEN) {
    return false;
  }
  return data[1] == ((id >> 5) & 0x3F);
}

static void handleIdNegotiation(uint16_t id, const uint8_t* buf, uint8_t len) {
  if (!isNegotiationFrame(id, buf, len)) {
    return;
  }

  uint8_t msgType = buf[0];
  uint8_t moduleType = buf[1];
  uint8_t instanceId = buf[2];

  if (msgType == ID_PROBE && moduleType == currentModuleType) {
    if (instanceId == currentInstanceId && currentInstanceId != 0) {
      uint8_t taken[3] = {ID_TAKEN, moduleType, instanceId};
      enqueueRaw(CAN_INSTANCE_ID(moduleType, 0x00), taken, 3, true);
    }
  } else if (msgType == ID_TAKEN &&
             moduleType == currentModuleType &&
             instanceId == neg.candidate &&
             (neg.phase == NegPhase::WaitReply ||
              neg.phase == NegPhase::Gap ||
              neg.phase == NegPhase::SendProbe ||
              neg.phase == NegPhase::Backoff)) {
    idConflictDetected = true;
  }
}

static void deliverFrame(const RxFrame& frame) {
  if (isNegotiationFrame(frame.id, frame.data, frame.len)) {
    handleIdNegotiation(frame.id, frame.data, frame.len);
    for (uint8_t i = 0; i < rawCallbackCount; i++) {
      if (rawCanCallbacks[i]) {
        rawCanCallbacks[i](frame.id, 0, frame.data, frame.len, frame.timestamp);
      }
    }
    return;
  }

  uint16_t senderId = 0;
  uint8_t payload[8];
  uint8_t payloadLen = frame.len;
  if (frame.len >= 2) {
    senderId = ((uint16_t)frame.data[0] << 8) | frame.data[1];
    payloadLen = frame.len - 2;
    memcpy(payload, &frame.data[2], payloadLen);
  } else {
    memcpy(payload, frame.data, frame.len);
  }

  for (uint8_t i = 0; i < rawCallbackCount; i++) {
    if (rawCanCallbacks[i]) {
      rawCanCallbacks[i](frame.id, senderId, payload, payloadLen, frame.timestamp);
    }
  }

  if (frame.id != thisModuleId && frame.id != CAN_ID_BROADCAST) {
    return;
  }

  for (uint8_t i = 0; i < callbackCount; i++) {
    if (canCallbacks[i]) {
      canCallbacks[i](frame.id, senderId, payload, payloadLen);
    }
  }

  if (senderId != 0) {
    enqueueLog(false, frame.id, senderId, payload, payloadLen);
  }
  counters.rxDelivered++;
}

static bool pushRx(uint16_t id, const uint8_t* data, uint8_t len) {
  if (rxCount >= kRxQueueCap) {
    counters.rxDropped++;
    noteDrop("RX queue full");
    return false;
  }
  uint8_t index = (rxHead + rxCount) % kRxQueueCap;
  RxFrame& frame = rxQueue[index];
  frame.id = id;
  frame.len = len;
  frame.timestamp = millis();
  memcpy(frame.data, data, len);
  rxCount++;
  return true;
}

static void drainController() {
  for (uint8_t n = 0; n < 8; n++) {
    uint8_t status = mcpReadStatus();
    if ((status & MCP_STAT_RXIF_MASK) == 0) {
      break;
    }
    if (rxCount >= kRxQueueCap) {
      canInterruptFlag = true;
      break;
    }

    const bool rx0 = (status & MCP_STAT_RX0IF) != 0;
    mcpBegin();
    SPI.transfer(rx0 ? MCP_READ_RX0 : MCP_READ_RX1);
    uint8_t raw[13];
    for (uint8_t i = 0; i < 13; i++) {
      raw[i] = SPI.transfer(0x00);
    }
    mcpEnd();

    if (raw[1] & MCP_TXB_EXIDE_M) {
      continue;
    }
    if (raw[4] & MCP_RTR_MASK) {
      continue;
    }

    uint8_t len = raw[4] & MCP_DLC_MASK;
    if (len > 8) {
      len = 8;
    }
    uint16_t id = ((uint16_t)raw[0] << 3) | (raw[1] >> 5);
    id &= 0x7FF;
    if (!pushRx(id, &raw[5], len)) {
      canInterruptFlag = true;
      break;
    }
  }
}

static void pollOverflow() {
  uint8_t flags = mcpReadReg(MCP_EFLG);
  uint8_t overflow = flags & (MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR);
  if (overflow == 0) {
    return;
  }
  if (overflow & MCP_EFLG_RX0OVR) {
    counters.rxHwOverflow++;
  }
  if (overflow & MCP_EFLG_RX1OVR) {
    counters.rxHwOverflow++;
  }
  mcpBegin();
  SPI.transfer(MCP_BITMOD);
  SPI.transfer(MCP_EFLG);
  SPI.transfer(MCP_EFLG_RX0OVR | MCP_EFLG_RX1OVR);
  SPI.transfer(0x00);
  mcpEnd();
  noteDrop("MCP2515 RX overflow");
}

static void dispatchReceived() {
  uint8_t budget = kMaxDispatchPerCall;
  while (budget > 0 && rxCount > 0) {
    RxFrame frame = rxQueue[rxHead];
    rxHead = (rxHead + 1) % kRxQueueCap;
    rxCount--;
    budget--;
    deliverFrame(frame);
  }
  if (rxCount > 0) {
    canInterruptFlag = true;
  }
}

void initCanBus(uint16_t fullCanId) {
  thisModuleId = fullCanId;
  pinMode(CAN_INT_PIN, INPUT);
  if (!isrAttached) {
    attachInterrupt(digitalPinToInterrupt(CAN_INT_PIN), onCanInterrupt, FALLING);
    isrAttached = true;
  }
  nextInitAttempt = 0;
  bringUpController();
}

void registerCanCallback(CanMessageCallback callback) {
  if (callbackCount < MAX_CAN_CALLBACKS) {
    canCallbacks[callbackCount++] = callback;
  }
}

void registerRawCanCallback(RawCanMessageCallback callback) {
  if (rawCallbackCount >= MAX_RAW_CAN_CALLBACKS) {
    return;
  }
  rawCanCallbacks[rawCallbackCount++] = callback;
  if (canBusInitialized && !applyAcceptanceFilters()) {
    canBusInitialized = false;
    nextInitAttempt = millis();
  }
}

void setCanBusLogging(bool enabled) {
  loggingEnabled = enabled;
}

CanBusCounters getCanBusCounters() {
  return counters;
}

bool isCanIdReady() {
  return canBusInitialized && neg.phase == NegPhase::Idle;
}

void handleCanMessages() {
  if (!canBusInitialized) {
    if (!due(nextInitAttempt)) {
      return;
    }
    if (!bringUpController()) {
      return;
    }
  }

  const bool nested = serviceDepth > 0;
  completeTransmissions();
  serviceNegotiation();
  submitTransmissions();
  canInterruptFlag = false;
  drainController();
  pollOverflow();

  if (nested) {
    if (rxCount > 0 || !digitalRead(CAN_INT_PIN)) {
      canInterruptFlag = true;
    }
    return;
  }

  serviceDepth++;
  dispatchReceived();
  serviceDepth--;
  completeTransmissions();
  submitTransmissions();
  printOneLog();

  if (rxCount > 0 || !digitalRead(CAN_INT_PIN) || canInterruptFlag) {
    canInterruptFlag = true;
  }
}

bool sendCanMessage(uint16_t receiverId, const uint8_t* data, uint8_t dataLen) {
  if (data == nullptr || dataLen == 0 || dataLen > 6) {
    counters.txDropped++;
    noteDrop("bad TX length");
    return false;
  }
  if (!enqueueUser(receiverId, data, dataLen)) {
    return false;
  }
  if (canBusInitialized && serviceDepth == 0) {
    completeTransmissions();
    submitTransmissions();
  }
  return true;
}

const char* getMessageTypeName(uint8_t msgType) {
  switch (msgType) {
    case ID_PROBE: return "ID_PROBE";
    case ID_TAKEN: return "ID_TAKEN";
    case TIMER_GAME_START: return "GAME_START";
    case TIMER_GAME_STOP: return "GAME_STOP";
    case TIMER_STRIKES: return "STRIKES";
    case TIMER_SERIAL_NUMBER_FIRST_HALF: return "SERIAL_FIRST_HALF";
    case TIMER_SERIAL_NUMBER_LAST_HALF: return "SERIAL_LAST_HALF";
    case TIMER_RESET: return "RESET";
    case TIMER_TIME: return "TIME";
    case TIMER_COUNTDOWN: return "COUNTDOWN";
    case TIMER_MODULE_DISCOVERED: return "MODULE_DISCOVERED";
    case MODULE_REGISTER: return "REGISTER";
    case MODULE_STRIKE: return "STRIKE";
    case MODULE_SOLVED: return "SOLVED";
    case MODULE_STATUS: return "STATUS";
    case MODULE_HEARTBEAT: return "HEARTBEAT";
    case MODULE_TELEMETRY: return "TELEMETRY";
    case SERIAL_DISPLAY_SET_SERIAL: return "SERIAL_DISPLAY_SET_SERIAL";
    case SERIAL_DISPLAY_CLEAR: return "SERIAL_DISPLAY_CLEAR";
    case SERIAL_DISPLAY_SHOW_CREDIT: return "SERIAL_DISPLAY_SHOW_CREDIT";
    case AUDIO_SET_VOLUME: return "AUDIO_SET_VOLUME";
    case AUDIO_PLAY_TITLE: return "AUDIO_PLAY_TITLE";
    case AUDIO_PLAY_BEEP_NORMAL: return "AUDIO_PLAY_BEEP_NORMAL";
    case AUDIO_PLAY_BEEP_FAST: return "AUDIO_PLAY_BEEP_FAST";
    case AUDIO_PLAY_BEEP_HIGH: return "AUDIO_PLAY_BEEP_HIGH";
    case AUDIO_PLAY_STRIKE: return "AUDIO_PLAY_STRIKE";
    case AUDIO_PLAY_DEFUSED: return "AUDIO_PLAY_DEFUSED";
    case AUDIO_PLAY_EXPLODED: return "AUDIO_PLAY_EXPLODED";
    case AUDIO_PLAY_CORRECT_TIME: return "AUDIO_PLAY_CORRECT_TIME";
    case AUDIO_PLAY_GAME_OVER_FANFARE: return "AUDIO_PLAY_GAME_OVER_FANFARE";
    case AUDIO_PLAY_ALARM_CLOCK_BEEP: return "AUDIO_PLAY_ALARM_CLOCK_BEEP";
    case AUDIO_PLAY_ALARM_CLOCK_SNOOZE: return "AUDIO_PLAY_ALARM_CLOCK_SNOOZE";
    case AUDIO_PLAY_ALARM_EMERGENCY: return "AUDIO_PLAY_ALARM_EMERGENCY";
    case AUDIO_PLAY_SIMON_RED: return "AUDIO_PLAY_SIMON_RED";
    case AUDIO_PLAY_SIMON_GREEN: return "AUDIO_PLAY_SIMON_GREEN";
    case AUDIO_PLAY_SIMON_YELLOW: return "AUDIO_PLAY_SIMON_YELLOW";
    case AUDIO_PLAY_SIMON_BLUE: return "AUDIO_PLAY_SIMON_BLUE";
    default: return "UNKNOWN";
  }
}

const char* getModuleTypeName(uint8_t moduleType) {
  switch (moduleType) {
    case CAN_TYPE_TIMER: return "TIMER";
    case CAN_TYPE_AUDIO: return "AUDIO";
    case CAN_TYPE_SIMON: return "SIMON";
    case CAN_TYPE_SERIAL_DISPLAY: return "SERIAL_DISPLAY";
    case CAN_TYPE_BROADCAST: return "BROADCAST";
    default: return "UNKNOWN";
  }
}

bool negotiateInstanceId(uint8_t moduleType, uint8_t* assignedId) {
  if (assignedId == nullptr || thisModuleId == CAN_ID_TIMER) {
    return false;
  }
  if (neg.phase == NegPhase::Idle && currentModuleType == moduleType && currentInstanceId != 0 && isCanIdReady()) {
    *assignedId = currentInstanceId;
    return true;
  }
  if (neg.phase == NegPhase::Idle) {
    assignUniqueId(moduleType);
  }
  return false;
}

bool assignUniqueId(uint8_t moduleType) {
  if (thisModuleId == CAN_ID_TIMER) {
    return false;
  }

  static bool seeded = false;
  if (!seeded) {
    randomSeed(rp2040.hwrand32());
    seeded = true;
  }

  if (neg.phase != NegPhase::Idle && neg.moduleType == moduleType) {
    return true;
  }

  currentModuleType = moduleType;
  currentInstanceId = 0;
  idConflictDetected = false;
  neg.moduleType = moduleType;
  neg.candidate = 1;
  neg.probesDone = 0;
  neg.confirming = false;
  neg.deadline = millis() + 50 + (unsigned long)random(0, 450);
  neg.phase = NegPhase::Startup;
  return true;
}

void updateCanId(uint16_t newCanId) {
  thisModuleId = newCanId;
}

uint8_t getCurrentInstanceId() {
  return thisModuleId & 0x1F;
}

uint16_t getCurrentModuleId() {
  return thisModuleId;
}

void updateModuleConnections() {
  unsigned long now = millis();

  if (audioModuleConnected && (now - lastAudioPing > MODULE_TIMEOUT_MS)) {
    audioModuleConnected = false;
  }

  if (serialDisplayConnected && (now - lastSerialDisplayPing > MODULE_TIMEOUT_MS)) {
    serialDisplayConnected = false;
  }
}

void decodeCanId(uint16_t canId, uint8_t* moduleType, uint8_t* instanceId) {
  *moduleType = (canId >> 5) & 0x3F;
  *instanceId = canId & 0x1F;
}

void logCanMessage(const char* direction, uint16_t receiverId, uint16_t senderId, uint16_t decodeId, const uint8_t* data, uint8_t len) {
  if (decodeId == 0) {
    return;
  }

  uint8_t moduleType, instanceId;
  decodeCanId(decodeId, &moduleType, &instanceId);

  unsigned long ttl = millis();
  KTANE_CONSOLE_OUT.print("[");
  KTANE_CONSOLE_OUT.print(ttl / 1000);
  KTANE_CONSOLE_OUT.print("s] ");
  KTANE_CONSOLE_OUT.print("CAN ");
  KTANE_CONSOLE_OUT.print(direction);
  KTANE_CONSOLE_OUT.print(": ID=0x");
  KTANE_CONSOLE_OUT.print(receiverId, HEX);
  KTANE_CONSOLE_OUT.print(" | Sender=0x");
  KTANE_CONSOLE_OUT.print(senderId, HEX);
  KTANE_CONSOLE_OUT.print(" | Module=");
  KTANE_CONSOLE_OUT.print(getModuleTypeName(moduleType));
  KTANE_CONSOLE_OUT.print(" | Instance=");
  KTANE_CONSOLE_OUT.print(instanceId);
  KTANE_CONSOLE_OUT.print(" | Len=");
  KTANE_CONSOLE_OUT.print(len);

  if (len > 0 && data != nullptr) {
    const char* commandName = getMessageTypeName(data[0]);
    KTANE_CONSOLE_OUT.print(" | Command=");
    KTANE_CONSOLE_OUT.print(commandName);
    KTANE_CONSOLE_OUT.print(" (0x");
    if (data[0] < 0x10) KTANE_CONSOLE_OUT.print("0");
    KTANE_CONSOLE_OUT.print(data[0], HEX);
    KTANE_CONSOLE_OUT.print(")");
  }

  KTANE_CONSOLE_OUT.print(" | Data=");

  for (uint8_t i = 0; i < len; i++) {
    KTANE_CONSOLE_OUT.print("0x");
    if (data[i] < 0x10) KTANE_CONSOLE_OUT.print("0");
    KTANE_CONSOLE_OUT.print(data[i], HEX);
    if (i < len - 1) KTANE_CONSOLE_OUT.print(" ");
  }
  KTANE_CONSOLE_OUT.println();
}
