# can_bus library

`shared_libs/can_bus/`. Include `<can_bus.h>`. The folder is compiled because each module sets `lib_extra_dirs = ../shared_libs`. There is no `library.properties`.

The wire format, id map, and message tables are in [CAN protocol](../architecture/can-protocol.md). This page is how a sketch calls the library.

Gameplay, audio, the serial tag, and the timer do not include `mcp_can.h`. The harness sketches under `test_modules/` do, and they are outside this guide.

## Calls

```cpp
void initCanBus(uint16_t fullCanId);
void handleCanMessages();
bool sendCanMessage(uint16_t receiverID, const uint8_t* data, uint8_t dataLen);
void registerCanCallback(CanMessageCallback callback);
void registerRawCanCallback(RawCanMessageCallback callback);
bool assignUniqueId(uint8_t moduleType);
bool isCanIdReady();
uint16_t getCurrentModuleId();
```

`initCanBus` programs the MCP2515 (500 kbit/s, 8 MHz, CS GP17, INT GP20, one-shot TX) and attaches a falling-edge ISR. It does not block if `begin` fails. `handleCanMessages` retries about once a second.

Call `handleCanMessages()` every pass of exactly one core. It drains RX, loads free TX buffers, and advances id negotiation. It is not safe from both cores. Do not `delay` on that core.

`sendCanMessage` queues 1..6 bytes and returns. `false` means the frame was not queued. The sender id is stamped when a TX buffer is loaded, so a frame queued during negotiation leaves with the final id.

The callback is `void(uint16_t id, uint16_t senderId, const uint8_t* data, uint8_t len)`. `id` is the destination. `data[0]` is the message type. The two-byte sender prefix has already been removed.

Up to 8 normal callbacks. They see frames addressed to this module or to `CAN_ID_BROADCAST`. Up to 2 raw callbacks. Those see every id, and registering one opens the hardware masks. Leave raw callbacks to a sniffer.

## Fixed id or negotiated id

```cpp
// Timer, audio, serial tag, future edgework panels
initCanBus(CAN_ID_AUDIO);

// Gameplay and needy modules
initCanBus(CAN_INSTANCE_ID(CAN_TYPE_WIRES, 0x00));
assignUniqueId(CAN_TYPE_WIRES);
```

`assignUniqueId` returns as soon as the first probe is scheduled. `isCanIdReady()` stays false until negotiation finishes inside `handleCanMessages`. `module_state` will not register before that. `assignUniqueId` returns false on the timer.

`updateCanId` is called from inside `can_bus.cpp`. Modules should not call it. Hardware filters match the whole type, so they stay put when the instance changes.

## Logging

TX/RX logs are off until `setCanBusLogging(true)`. `handleCanMessages` prints at most one queued line per call. Negotiation frames are not logged. `getCanBusCounters()` returns delivered, overflow, retry, and drop counts.

`getModuleTypeName` only knows timer, audio, simon, serial display, and broadcast. Extend it when you add a type.

`audioModuleConnected` and `serialDisplayConnected` drop after 5 seconds without traffic (`MODULE_TIMEOUT_MS`). Track new modules on the timer through heartbeats, not by adding another global here.
