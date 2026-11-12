# Modules

Bomb firmware: the timer master plus solvable puzzles. Each folder is its own PlatformIO project. Clone `simon_says/` when adding the next manual module.

| Folder | Role |
|--------|------|
| `timer/` | Master controller (not a solvable module) |
| `simon_says/` | Reference gameplay module |

Build from a module directory: `pio run`, or from the repo root: `./build_module.sh simon_says`.
