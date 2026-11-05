// nfsc - the monitor the game opens on, for settings that follow it.

#pragma once

#include <cstdint>

namespace nfsc {

struct DisplayMode {
  uint32_t width = 1920;
  uint32_t height = 1080;
  double refresh_rate = 60.0;
};

// The current mode of the monitor selected by the monitor cvar. Read once.
const DisplayMode& CurrentDisplay();

// The frame rate that keeps every frame on screen equally long: the refresh
// rate, or a whole fraction of it on monitors faster than the game gets.
double MonitorFrameRate();

// The resolution scale that covers the monitor (1 = 720 lines).
int MonitorResolutionScale();

}  // namespace nfsc
