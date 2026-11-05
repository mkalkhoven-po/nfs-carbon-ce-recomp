// nfsc - the monitor the game opens on (see display.h).

#include "display.h"

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include <rex/cvar.h>
#include <rex/logging.h>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace nfsc {

namespace {

// Races average ~125 fps on an RTX 5080 without a limit. On faster monitors an
// even 120 looks better than a frame rate that keeps missing refreshes.
constexpr double kMaxMonitorFrameRate = 165.0;
// The most tested so far; 3x is 2160 lines.
constexpr int kMaxResolutionScale = 3;

#if defined(_WIN32)
// GDI device names, primary monitor first like the monitor cvar's order.
BOOL CALLBACK AddMonitor(HMONITOR monitor, HDC, LPRECT, LPARAM names) {
  auto& list = *reinterpret_cast<std::vector<std::wstring>*>(names);
  MONITORINFOEXW info = {};
  info.cbSize = sizeof(info);
  if (GetMonitorInfoW(monitor, &info)) {
    list.insert((info.dwFlags & MONITORINFOF_PRIMARY) ? list.begin() : list.end(), info.szDevice);
  }
  return TRUE;
}

// The exact refresh rate (59.94 rather than the 59 Windows rounds it to), or 0.
double ExactRefreshRate(const std::wstring& device) {
  UINT32 path_count = 0, mode_count = 0;
  if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &path_count, &mode_count) !=
      ERROR_SUCCESS) {
    return 0.0;
  }
  std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
  std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
  if (QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &path_count, paths.data(), &mode_count,
                         modes.data(), nullptr) != ERROR_SUCCESS) {
    return 0.0;
  }
  for (UINT32 i = 0; i < path_count; ++i) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME name = {};
    name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    name.header.size = sizeof(name);
    name.header.adapterId = paths[i].sourceInfo.adapterId;
    name.header.id = paths[i].sourceInfo.id;
    const auto& rate = paths[i].targetInfo.refreshRate;
    if (DisplayConfigGetDeviceInfo(&name.header) == ERROR_SUCCESS &&
        device == name.viewGdiDeviceName && rate.Denominator != 0) {
      return double(rate.Numerator) / double(rate.Denominator);
    }
  }
  return 0.0;
}
#endif

DisplayMode ReadDisplay() {
  DisplayMode mode;
#if defined(_WIN32)
  std::vector<std::wstring> names;
  EnumDisplayMonitors(nullptr, nullptr, AddMonitor, reinterpret_cast<LPARAM>(&names));
  const size_t index = size_t(std::max(rex::cvar::Query<int32_t>("monitor") - 1, 0));
  if (index >= names.size()) {
    return mode;
  }
  DEVMODEW settings = {};
  settings.dmSize = sizeof(settings);
  if (EnumDisplaySettingsW(names[index].c_str(), ENUM_CURRENT_SETTINGS, &settings)) {
    mode.width = settings.dmPelsWidth;
    mode.height = settings.dmPelsHeight;
    if (settings.dmDisplayFrequency > 1) {
      mode.refresh_rate = settings.dmDisplayFrequency;
    }
  }
  if (const double exact = ExactRefreshRate(names[index]); exact > 1.0) {
    mode.refresh_rate = exact;
  }
#endif
  return mode;
}

}  // namespace

const DisplayMode& CurrentDisplay() {
  static const DisplayMode mode = [] {
    const DisplayMode read = ReadDisplay();
    REXLOG_INFO("nfsc: monitor {}x{} at {:.2f} Hz", read.width, read.height, read.refresh_rate);
    return read;
  }();
  return mode;
}

double MonitorFrameRate() {
  const double refresh_rate = CurrentDisplay().refresh_rate;
  return refresh_rate / std::ceil(refresh_rate / kMaxMonitorFrameRate);
}

int MonitorResolutionScale() {
  const int scale = int(std::ceil(CurrentDisplay().height / 720.0));
  return std::clamp(scale, 1, kMaxResolutionScale);
}

}  // namespace nfsc
