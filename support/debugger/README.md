# CAN debugger harnesses

Four Pico W bench sketches for MCP2515 wiring and KTANE framing. Built separately: `./build_debug.ps1` or `./build_debug.sh`.

| Sketch | Purpose |
|--------|---------|
| `can_test_send` / `can_test_receive` | Raw `MCP_CAN` loopback / listen |
| `can_debug_send` / `can_debug_receive` | `can_bus` registration and traffic watch |

See `.cursor/skills/module-can-harness/SKILL.md` for wiring and pairing with `modules/timer`.
