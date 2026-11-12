#pragma once

#include <stdint.h>

#define KTANE_AUDIO_BLOCK_SAMPLES 256
#define KTANE_AUDIO_STREAM_SLOTS 6

#ifdef __cplusplus
extern "C" {
#endif

// Core 1 (I/O) -> core 0 (mixer): SPSC command queue. Returns 1 on success, 0 if full.
void audioMixerIpcInit(void);
int audioMixerIpcTryPushPlayRam(const int16_t* data, uint32_t length_samples);
int audioMixerIpcTryPushSetVolume(uint8_t percent);
int audioMixerIpcTryPushStreamArm(uint8_t slot);
int audioMixerIpcTryPushStreamStop(uint8_t slot);

// Stream ring: core 1 produces mono PCM blocks; core 0 consumes. slot < 6.
int audioStreamRingTryPushBlock(uint8_t slot, const int16_t* mono_block);
void audioStreamRingReset(uint8_t slot);
int audioMixerStreamSlotIsIdle(uint8_t slot);

#ifdef __cplusplus
}
#endif
