---
name: mcp-can
description: >-
  Use coryjfowler/MCP_CAN_lib (MCP2515) the way KTANE does: 8 MHz crystal,
  500 kbit/s, standard 11-bit frames. Use when editing can_test_send,
  can_test_receive, or shared_libs/can_bus, or when a module fails to init
  the CAN controller. Gameplay code sends through can_bus.h, not MCP_CAN.
---

# MCP_CAN (coryjfowler)

PlatformIO: `coryjfowler/mcp_can@^1.5.1`. Include `<mcp_can.h>` and `<SPI.h>`.

This is the MCP2515 driver. Gameplay, audio, serial, and the timer do not call it. They call `initCanBus` / `sendCanMessage` / `handleCanMessages`. Use `MCP_CAN` directly only in `can_test_send/`, `can_test_receive/`, and inside `shared_libs/can_bus/can_bus.cpp`.

The boards on this bench have an 8 MHz crystal. `MCP_16MHZ` will init and then see nothing. Confirm the crystal if a new board is silent.

## Init that matches the bus

```cpp
#include <SPI.h>
#include <mcp_can.h>

#define CAN0_CS  17   // GP17, SPI0
#define CAN0_INT 20   // GP20, active low

MCP_CAN can(CAN0_CS);

void setup() {
  SPI.begin(); // SPI0 defaults: MISO 16, SCK 18, MOSI 19
  if (can.begin(MCP_ANY, CAN_500KBPS, MCP_8MHZ) != CAN_OK) {
    // begin() failed: wiring, power, or wrong crystal constant
  }
  // MCP_ANY leaves masks open. Still program them; some silicon boots with garbage.
  can.init_Mask(0, 0, 0x00000000);
  can.init_Mask(1, 0, 0x00000000);
  for (int i = 0; i < 6; i++) can.init_Filt(i, 0, 0x00000000);
  can.enOneShotTX();          // don't retry forever when nobody ACKs
  can.setMode(MCP_NORMAL);    // begin() leaves the controller in loopback
}
```

`begin(uint8_t idmodeset, uint8_t speedset, uint8_t clockset)`:

- `idmodeset`: `MCP_STDEXT` (masks apply), `MCP_ANY` (accept all), `MCP_STD`, `MCP_EXT`.
- `speedset`: `CAN_500KBPS` on this bus. Also defined: `CAN_5KBPS` through `CAN_1000KBPS`, plus `CAN_31K25BPS`, `CAN_33K3BPS`, `CAN_40KBPS`, `CAN_50KBPS`, `CAN_80KBPS`, `CAN_100KBPS`, `CAN_125KBPS`, `CAN_200KBPS`, `CAN_250KBPS`.
- `clockset`: `MCP_8MHZ` here. Also `MCP_16MHZ`, `MCP_20MHZ`.

`setMode`: `MCP_NORMAL`, `MCP_SLEEP`, `MCP_LOOPBACK`, `MCP_LISTENONLY`. Loopback needs no transceiver. Listen-only is the right mode for a probe that must not ACK.

Return codes: `CAN_OK` 0, `CAN_FAILINIT` 1, `CAN_FAILTX` 2, `CAN_MSGAVAIL` 3, `CAN_NOMSG` 4, `CAN_CTRLERROR` 5, `CAN_GETTXBFTIMEOUT` 6, `CAN_SENDMSGTIMEOUT` 7, `CAN_FAIL` 0xFF.

## Send and receive

Standard 11-bit frames only. The second argument of `sendMsgBuf` is the extended-id flag, and it stays 0.

```cpp
byte payload[8] = {0};
byte ok = can.sendMsgBuf(0x100, 0, 8, payload);

if (can.checkReceive() == CAN_MSGAVAIL) {
  unsigned long id;
  unsigned char len = 0;
  unsigned char buf[8];
  can.readMsgBuf(&id, &len, buf);
  // id & 0x80000000  extended id
  // id & 0x40000000  remote request
  id &= 0x7FF;
}
```

`readMsgBuf(&id, &ext, &len, buf)` returns the extended flag separately and does not report remote frames. `can_bus` uses the 3-argument form.

`getCanId()` is gone. Do not call it.

Poll INT (active low) or `checkReceive()`. `can_bus` does both: a falling-edge interrupt sets a flag, and `handleCanMessages` also reads the pin, because a flag can be missed.

## Errors

```cpp
byte err = can.getError();   // bit0 TXWAR, bit1 RXWAR, bit2 TXBO, bit3 RXBO, bit4 EPASS, bit5 EWARN
can.errorCountTX();
can.errorCountRX();
can.checkError();
```

Bus-off usually means missing termination, a third J1 jumper, or a speed/crystal mismatch. `can_test_send` recovers by strobing CS with SPI byte `0xC0` (MCP2515 reset), then `begin` + `setMode(MCP_NORMAL)` again.

## Masks

Two masks, six filters. `init_Mask(num, ext, data)` and `init_Filt(num, ext, data)`. For standard ids the library shifts the 11-bit id to the top of the 32-bit register; copy `examples/Standard_MaskFilter` rather than inventing the shift. `can_bus` programs the masks itself after `begin(MCP_STDEXT, ...)`: RXB0 matches the module type (instance bits ignored) and RXB1 matches `CAN_ID_BROADCAST`. `handleCanMessages` still delivers only `thisModuleId` or `CAN_ID_BROADCAST`. A raw callback opens both masks.

## Wiring shared by every module

| Signal | GPIO |
|---|---|
| CS | GP17 |
| INT | GP20 |
| MISO | GP16 |
| SCK | GP18 |
| MOSI | GP19 |
| Crystal | 8 MHz |
| Bit rate | 500 kbit/s |

J1 (on-board 120 ohm) is fitted on the two nodes at the ends of the twisted pair only.
