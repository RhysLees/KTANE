#pragma once

#include <stdint.h>

// Harness-only payloads (data[0]). Not part of the bomb manual protocol.

enum CanBusTestCommand : uint8_t {
  TEST_CMD_PING = 0xF0,
  TEST_CMD_PONG = 0xF1,
  TEST_CMD_START_RX_FLOOD = 0xF2,
  TEST_CMD_RX_FLOOD_DONE = 0xF3,
  TEST_CMD_START_TX_BURST = 0xF4,
  TEST_CMD_TX_BURST_DONE = 0xF5,
  TEST_CMD_REPORT_STATS = 0xF6,
  TEST_CMD_STATS = 0xF7,
  TEST_CMD_NONBLOCK_PROBE = 0xF8,
  TEST_CMD_NONBLOCK_RESULT = 0xF9,
};

constexpr uint8_t CAN_BUS_TEST_SLAVE_TYPE = 0x3D; // CAN_TYPE_DEBUGGER_SENDER

// Override via master platformio build_flags (see can_test_master_clear env).

inline uint16_t packTimeMs24(uint32_t ms) {
  return (uint16_t)(ms & 0xFFFF);
}
