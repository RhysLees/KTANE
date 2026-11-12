# Simon Says setup

Pico (not Pico W), Picoprobe on GP0/GP1, CAN on SPI0, four buttons, four LEDs, one status LED. Shared controller pins are in [Bus wiring](../../getting-started/bus-wiring.md).

## Wiring

```text
GP0  UART0 TX -> Picoprobe RX
GP1  UART0 RX -> Picoprobe TX

GP2  red button      to GND, internal pull-up
GP3  green button    to GND, internal pull-up
GP4  blue button     to GND, internal pull-up
GP5  yellow button   to GND, internal pull-up

GP6  red LED
GP7  green LED
GP8  blue LED
GP9  yellow LED
GP11 status LED

GP16..GP20 MCP2515
```

Buttons are active low. LEDs are active high. `begin()` turns every color LED on once during init, then the phase machine owns them. The status LED flashes 500 ms on / 500 ms off until the timer sends `TIMER_MODULE_DISCOVERED`, then stays off except for a 1 second flash when the strike count changes. Solved is shown on the four color LEDs, not by holding the status LED on.

Leave GP0 and GP1 free. Simon does not use I2C or SPI1.

## Flash

```powershell
cd simon_says
pio run --target upload
pio device monitor
```

`board` is `rpipico`. `lib_deps` is only `mcp_can`. Shared code comes from `lib_extra_dirs = ../shared_libs`.

## Bring-up

1. Power the timer and this board on the same terminated CAN pair.
2. The status LED flashes until the timer page lists a Simon module. The negotiated id is type `0x13` with instance 1 or higher. Instance 1 is id `0x261`. A second Simon takes the next free instance.
3. Start a game from the timer. The module stays idle until `TIMER_GAME_START`.
4. The first stage is one flash. Press the color from [Rules](rules.md). A correct stage grows the sequence. A wrong press is one strike and a 1 second flash of the color LEDs. Waiting 5 seconds replays the stage and is not a strike.
5. Five completed stages send `MODULE_SOLVED`. The timer plays `AUDIO_PLAY_CORRECT_TIME` from its own solved callback. Simon also sends `AUDIO_PLAY_DEFUSED` from the puzzle.

If the LED never stops flashing, the timer is not answering `MODULE_REGISTER`. Check termination, the 8 MHz crystal constant, and that `handleCanMessages()` is running. Simon's `loop` calls it on every pass, on core 0.

## Copying this module

See [Adding a module](../../architecture/adding-a-module.md). Keep puzzle GPIO in the library and CAN status in `main.cpp`.
