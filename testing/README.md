# KTANE testing

Hardware and protocol benches that are not bomb modules. Placeholder dirs mirror production layout so future module-specific tests can land beside the code they exercise.

| Path | Purpose |
|------|---------|
| `edgework/` | Edgework panel harnesses (future) |
| `modules/` | Gameplay module harnesses (future) |
| `support/` | Support-module harnesses (future) |
| `debugging/` | Bus and bring-up tools |

Build CAN bus pair: `./build_test.ps1` (master + slave under `debugging/can_bus/`).

Wiring matches production: MCP2515 SPI0 CS GP17, INT GP20, 8 MHz crystal, 500 kbit/s. Fit the 120 Ω terminator (J1) on **both** Picos when they are the only nodes on the bus. CAN test harnesses log over **USB** (no Picoprobe).
