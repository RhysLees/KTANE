# Audio setup

Pico W, Picoprobe on GP0/GP1, CAN on SPI0, TLV320DAC3100 on I2C1, I2S from the RP2040, SD card on SPI1. Shared controller pins are in [Bus wiring](../../getting-started/bus-wiring.md).

Use this sketch on the bomb. Use [`audio_test`](../audio-test/README.md) when you are proving a file, a clock, or a buffer size and you want the bus left quiet.

## Wiring

```text
GP0  UART0 TX -> Picoprobe RX
GP1  UART0 RX -> Picoprobe TX

GP3  I2S BCLK  -> TLV320 BCLK
GP4  I2S WSEL  -> TLV320 WSEL / LRCLK
GP5  I2S DIN   -> TLV320 DIN

GP6  I2C1 SDA  -> TLV320 SDA
GP7  I2C1 SCL  -> TLV320 SCL
GP8  GPIO      -> TLV320 RST, active low

GP12 SPI1 MISO -> SD DO / MISO
GP13 SPI1 CS   -> SD CS
GP14 SPI1 SCK  -> SD SCK
GP15 SPI1 MOSI -> SD DI / MOSI

GP16..GP20 MCP2515 on SPI0
```

Headphones plug into the codec's headphone output, not a PWM pin. Reset is active low. Leave it under firmware control (`resetAmp` / the two-phase init). Do not tie it high and also let GP8 drive it.

`audio/lib/audio/audio.h` documents an older I2S set (GP6/GP7/GP8). Those pins are the codec's I2C and reset on the board this sketch boots. Wire the table above.

## SD card

Format FAT. Put files at the volume root under `sounds/`, matching [Sound bank](sound-bank.md). The loader and the streamer both want 16-bit PCM at 44100 Hz. Mono or stereo both decode. The stream path rejects any other sample rate.

SPI starts at 10 MHz on `SPI1` and falls back to 1 MHz if the card will not enumerate. `printSdCardInfo` runs from `setup` when the card opens.

Both cores can touch the card. Every SdFat call in this module takes `audioSdLock()`. A new helper that reads the card from core 0 or core 1 has to take the same lock.

## Flash

```powershell
cd support/audio
pio run --target upload
pio device monitor
```

`board` is `rpipicow`. First build clones `arduino-audio-tools` and `Adafruit_TLV320_I2S`. `-DEDGE_AUDIO` must stay set for a bomb build.

## Bring-up

1. Insert the card, flash, and open the probe console at 115200.
2. Expect, in order: CAN ready, module state ready, mixer ready with a volume percent (default 20), then the SD summary and `Ready.`
3. `I2S begin failed` means BCLK/WSEL/DIN or the codec reset. Fix that before looking at the SD card. The mixer is started first so a codec fault is visible while SPI1 is still idle.
4. Power the timer on the same CAN pair. The audio module registers at `0x020` with no id negotiation. It is not a gameplay module, so it does not appear in `GET /api/modules`. The setup page reads `status.audioModules` from `GET /api/all`. `GET /api/audio` returns `connected: true` once a heartbeat has been seen.
5. Start a game. The countdown sends `AUDIO_PLAY_BEEP_NORMAL` once a second. You should hear the short beep from `/sounds/double_beep.wav`.
6. Volume from the timer page is `POST /api/audio` with `{ "volume": 40 }`. That becomes `AUDIO_SET_VOLUME` on the bus. 0 is silent PCM. 100 is full scale in the mixer. It does not change the codec's headphone PGA (`setAmpHeadphoneVolume`, 0..63), which the boot path leaves at the library default.

Bluetooth is compiled and unused. Playback is I2S. Do not call the A2DP helpers from `loop` without giving them a core that is not the I2S mixer.

If CAN is up and the timer never marks the module connected, check the 8 MHz crystal and that core 1 is the one calling `handleCanMessages`. Under `EDGE_AUDIO`, core 0 does not touch the controller.
