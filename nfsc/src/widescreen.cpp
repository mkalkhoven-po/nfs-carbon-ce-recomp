// nfsc - custom render resolution (widescreen / ultrawide), work in progress.
//
// Reverse engineered from default.xex (title 454107EC, v0.0.0.7):
//   sub_8250B0D0  picks the render size from the video mode: widescreen with a
//                 width >= 1280 gives 1280x720, narrower 1024x576. It stores the
//                 size twice, in 0x82C651E8/EC and 0x82C651F0/F4.

#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

REXCVAR_DEFINE_INT32(render_width, 0, "NFSC",
                     "Game render width in pixels (0 = the game's own 1280); experimental")
    .range(0, 3840)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_INT32(render_height, 0, "NFSC",
                     "Game render height in pixels (0 = the game's own 720); experimental")
    .range(0, 2160)
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REX_EXTERN(__imp__sub_8250B0D0);

namespace {

constexpr uint32_t kRenderSizes[][2] = {{0x82C651E8, 0x82C651EC}, {0x82C651F0, 0x82C651F4}};

uint32_t LoadU32(uint8_t* base, uint32_t addr) {
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + addr));
}

void StoreU32(uint8_t* base, uint32_t addr, uint32_t value) {
  *reinterpret_cast<uint32_t*>(base + addr) = __builtin_bswap32(value);
}

}  // namespace

REX_HOOK_RAW(sub_8250B0D0) {
  __imp__sub_8250B0D0(ctx, base);
  const int32_t width = REXCVAR_GET(render_width);
  const int32_t height = REXCVAR_GET(render_height);
  if (width <= 0 && height <= 0) {
    return;
  }
  for (const auto& size : kRenderSizes) {
    const uint32_t old_width = LoadU32(base, size[0]);
    const uint32_t old_height = LoadU32(base, size[1]);
    if (old_width != 1280 || old_height != 720) {
      REXLOG_WARN("nfsc: unexpected render size {}x{}, not overriding", old_width, old_height);
      return;
    }
  }
  const uint32_t new_width = width > 0 ? uint32_t(width) : 1280;
  const uint32_t new_height = height > 0 ? uint32_t(height) : 720;
  for (const auto& size : kRenderSizes) {
    StoreU32(base, size[0], new_width);
    StoreU32(base, size[1], new_height);
  }
  REXLOG_INFO("nfsc: render size 1280x720 -> {}x{}", new_width, new_height);
}
