#pragma once

#include <Arduino.h>

#if defined(EDGE_AUDIO_CONSOLE)
#define KTANE_CONSOLE_OUT Serial1
#else
#define KTANE_CONSOLE_OUT Serial
#endif
