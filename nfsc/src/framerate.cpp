// nfsc - per-frame work on the guest's present, and an optional frame rate log.
//
// Reverse engineered from default.xex (title 454107EC, v0.0.0.7):
//   sub_826E3860  presents a frame (it calls VdSwap); called once per game frame.

#include <algorithm>
#include <chrono>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "widescreen.h"

REXCVAR_DEFINE_BOOL(log_fps, false, "NFSC",
                    "Log the game's frame rate (frames presented) once per second, with the "
                    "slowest frame of that second");

namespace {

void LogFrameRate() {
  using Clock = std::chrono::steady_clock;
  static Clock::time_point second_start = Clock::now();
  static Clock::time_point last_frame = second_start;
  static uint32_t frames = 0;
  static Clock::duration slowest{};
  const Clock::time_point now = Clock::now();
  slowest = std::max(slowest, now - last_frame);
  last_frame = now;
  ++frames;
  const auto elapsed = now - second_start;
  if (elapsed >= std::chrono::seconds(1)) {
    const double seconds = std::chrono::duration<double>(elapsed).count();
    REXLOG_INFO("nfsc: {:.1f} fps, slowest frame {:.1f} ms", frames / seconds,
                std::chrono::duration<double, std::milli>(slowest).count());
    second_start = now;
    frames = 0;
    slowest = {};
  }
}

}  // namespace

REX_EXTERN(__imp__sub_826E3860);
REX_HOOK_RAW(sub_826E3860) {
  nfsc::UpdatePresentAspect(base);
  if (REXCVAR_GET(log_fps)) {
    LogFrameRate();
  }
  __imp__sub_826E3860(ctx, base);
}
