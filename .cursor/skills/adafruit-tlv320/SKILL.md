---
name: adafruit-tlv320
description: >-
  Configure the TLV320DAC3100 headphone codec with Adafruit_TLV320DAC3100 on
  Wire1. Use when editing audio/lib/amp, audio bring-up, volume, speaker
  enable, or I2S clocks. powerPLL runs only after I2S BCLK is toggling.
---

# Adafruit TLV320DAC3100

`https://github.com/adafruit/Adafruit_TLV320_I2S.git`. Include `<Adafruit_TLV320DAC3100.h>`. The class is `Adafruit_TLV320DAC3100`. The chip needs I2C configuration before it makes sound; I2S data alone is silence.

Wrapper: `audio/lib/amp/`. Call `initAmpPhaseBeforeI2s`, start I2S, then `initAmpPhaseAfterI2s`. `initAmp()` does both phases and is only valid when BCLK is already running (a soft reset while audio is up).

## Pins

| Signal | GPIO | Bus |
|---|---|---|
| SDA | GP6 | `Wire1` (I2C1) |
| SCL | GP7 | `Wire1` |
| RST | GP8 | active low, hold low 100 ms then high |
| BCLK | GP3 | I2S, not I2C |
| WSEL | GP4 | I2S |
| DIN | GP5 | I2S |

I2C address is `TLV320DAC3100_I2CADDR_DEFAULT` (`0x18`). Some boards ACK at `0x19`. `amp.cpp` tries both. Scan with `Wire1.beginTransmission(addr); Wire1.endTransmission() == 0`.

```cpp
Wire1.setSDA(6);
Wire1.setSCL(7);
Wire1.begin();
Wire1.setClock(100000);

Adafruit_TLV320DAC3100 codec;
if (!codec.begin(TLV320DAC3100_I2CADDR_DEFAULT, &Wire1)) {
  codec.begin(0x19, &Wire1);
}
```

`begin(uint8_t addr, TwoWire* wire)` only probes I2C. The default wire pointer is `&Wire` (I2C0). Omitting `&Wire1` looks like a missing chip.

`codec.reset()` is a software reset. The GPIO reset in `amp.cpp` is the one that recovers a wedged part.

## Clock program for 44.1 kHz, 16-bit I2S, no MCLK

Phase 1, BCLK still stopped. These match `TLV320_Audio_Playback_Arduino` and `initAmpPhaseBeforeI2s`:

```cpp
codec.setCodecInterface(TLV320DAC3100_FORMAT_I2S, TLV320DAC3100_DATA_LEN_16);
codec.setCodecClockInput(TLV320DAC3100_CODEC_CLKIN_PLL);
codec.setPLLClockInput(TLV320DAC3100_PLL_CLKIN_BCLK);
codec.setPLLValues(/*P*/ 1, /*R*/ 1, /*J*/ 8, /*D*/ 0);
codec.setNDAC(true, 8);
codec.setMDAC(true, 2);
codec.setDOSR(128);
```

Every call returns `bool`. A false means the I2C write was NAK'd; stop and rescan the bus.

Phase 2, after `I2SStream.begin` so BCLK is present:

```cpp
codec.powerPLL(true);
delay(50);
codec.setDACDataPath(true, true,
                     TLV320_DAC_PATH_NORMAL, TLV320_DAC_PATH_NORMAL,
                     TLV320_VOLUME_STEP_1SAMPLE);
codec.configureAnalogInputs(TLV320_DAC_ROUTE_MIXER, TLV320_DAC_ROUTE_MIXER,
                            false, false, false, false);
codec.setDACVolumeControl(false, false, TLV320_VOL_INDEPENDENT);
codec.setChannelVolume(false, 0.0f);   // left, dB, range about -63.5 .. +24
codec.setChannelVolume(true, 0.0f);    // right
```

`setPLLValues(p, r, j, d)` programs PLL_P, PLL_R, PLL_J, PLL_D. Do not copy the `basicI2Sconfig` example's `(1, 2, 32, 0)`; that example assumes a different BCLK. This bomb's 44.1 kHz 16-bit stream uses `(1, 1, 8, 0)`, NDAC 8, MDAC 2, DOSR 128.

`setChannelVolume`'s second argument is decibels, not the 0..63 headphone register. `amp.cpp` comments this because the two scales are easy to swap.

## Headphones versus speaker

The shipping path is headphones, speaker forced off:

```cpp
// Adafruit configureHeadphoneDriver does not set HP_DRIVERS bit 2.
// Write page 1 register TLV320DAC3100_REG_HP_DRIVERS = 0x04 first,
// or the following read-modify-write misses the chip. See amp.cpp.
codec.configureHeadphoneDriver(true, true, TLV320_HP_COMMON_1_35V, false);
codec.configureHPL_PGA(/*gain dB*/ 0, /*unmute*/ true);
codec.configureHPR_PGA(0, true);
codec.setHPLVolume(true, 6);    // unmute, 0..63 register steps
codec.setHPRVolume(true, 6);
codec.enableSpeaker(false);
```

Speaker, if a later build wires the class-D pins (5 V supply, speaker only, never into an amp input):

```cpp
codec.enableSpeaker(true);
codec.configureSPK_PGA(TLV320_SPK_GAIN_6DB, true);
codec.setSpeakerVolume(true, volume0to63);
```

`setAmpHeadphoneVolume(rightChannel, 0..63)` and `setAmpSpeakerEnabled` in `amp.h` are the module API. CAN volume (`AUDIO_SET_VOLUME`, 0..100) scales samples in the mixer. It does not rewrite the codec PGA. Change the PGA when the analog level is wrong at mixer volume 100.

## Page writes

The chip is paged. `codec.begin` leaves you on page 0 for the helpers above. A raw write, which `amp.cpp` uses for the headphone-driver prime:

```cpp
Wire1.beginTransmission(TLV320DAC3100_I2CADDR_DEFAULT);
Wire1.write(TLV320DAC3100_REG_PAGE_SELECT);
Wire1.write(page);
Wire1.endTransmission();
Wire1.beginTransmission(TLV320DAC3100_I2CADDR_DEFAULT);
Wire1.write(reg);
Wire1.write(value);
Wire1.endTransmission();
```

Helpers cache nothing across a manual page change. Call them after you are done poking registers, or they will write the wrong page.

## Failure signs

- I2C scan empty: RST still low, wrong `TwoWire`, or GP6/GP7 not actually I2C1.
- Probe succeeds, no audio: `powerPLL` ran before BCLK, or `setPLLValues` does not match 44.1 kHz, or the mixer is writing zeros because the SD file was rejected.
- Headphones click but no tone: speaker path enabled and headphone drivers left muted.
