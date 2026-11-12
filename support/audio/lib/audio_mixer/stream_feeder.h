#pragma once

#if defined(EDGE_AUDIO)

// Core 1: read SD and fill stream rings for the mixer on core 0.
void audioStreamFeederPump(void);

#endif
