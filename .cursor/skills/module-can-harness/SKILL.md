---
name: module-can-harness
description: >-
  CAN bench sketches can_test_send, can_test_receive, can_debug_send, and
  can_debug_receive. Use when bringing up an MCP2515 or watching KTANE
  frames. Test sketches talk raw MCP_CAN. Debug sketches use can_bus.
---

# CAN harnesses

Four Pico W sketches, same `platformio.ini` shape: `board = rpipicow`, `-DMODULE_DEBUG`, `-DKTANE_PICOPROBE_UART`, `lib_deps` is only `mcp_can`, `lib_extra_dirs = ../shared_libs`. None of them is a puzzle. `-DMODULE_DEBUG` is not read by the sketches.

Wiring for all four: SPI0 CS GP17, INT GP20, 8 MHz crystal, 500 kbit/s. Console on GP0/GP1.

## can_test_send and can_test_receive

Raw `MCP_CAN`, copied from the library's `CAN_send` / `CAN_receive` examples and pointed at this hardware. They do not speak KTANE framing. Use them when `initCanBus` fails and you need to know whether the transceiver, the crystal constant, or the termination is at fault.

`can_test_send` checks `getError`, `errorCountTX`, `errorCountRX`, and can reset the controller with SPI byte `0xC0` plus `begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ)` and `setMode(MCP_NORMAL)`.

`can_test_receive` calls `SPI.begin()`, then the same `begin`, and prints every frame. `readMsgBuf` ids still carry `0x80000000` / `0x40000000` flags. Mask them off before comparing to a KTANE id.

A good test pair is one send sketch and one receive sketch on two boards, J1 fitted on both only because they are the two ends. Frames should show 500 kbit/s standard ids. If TX error counters climb, the other node is not ACKing: speed, crystal, or a missing terminator.

API: [mcp-can](../mcp-can/SKILL.md).

## can_debug_send

Uses `can_bus`, not raw `MCP_CAN`. Pretends to be a Simon module:

```cpp
initCanBus(CAN_INSTANCE_ID(CAN_TYPE_SIMON, 0x00));
assignUniqueId(CAN_TYPE_SIMON);
```

Until it sees `TIMER_MODULE_DISCOVERED` it sends `MODULE_REGISTER` to `CAN_ID_TIMER` once a second. Then it sends `MODULE_HEARTBEAT`. Pair it with a running timer. This is the check that negotiation and the 6-byte framed payload work, which the raw sketches cannot tell you.

## can_debug_receive

Also uses `can_bus`. Prints traffic addressed to it and tracks senders (id, type name, first seen, last seen, count) with a 5 s timeout, up to 8 senders. Use it beside the timer when a module should be heartbeating and the timer UI disagrees.

It will not see frames whose destination is some other id, unless you add `registerRawCanCallback`. That also opens the MCP2515 masks, so the debug node then has to read every frame. `handleCanMessages` drops other destinations before normal callbacks. A raw callback is the way to snoop the whole bus from this sketch.

## Which sketch to flash

| Question | Sketch |
|---|---|
| Does the MCP2515 answer on SPI? | `can_test_send` |
| Do two transceivers exchange any frame? | `can_test_send` + `can_test_receive` |
| Does this board get a unique Simon id and a discovery ack? | `can_debug_send` against `timer/` |
| Who is actually transmitting KTANE frames? | `can_debug_receive` |
