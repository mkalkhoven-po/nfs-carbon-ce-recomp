// nfsc - custom render resolution (widescreen / ultrawide), work in progress.
//
// Status: with a wider render size the front buffer, HUD and 3D viewport all
// widen, but the main scene shows black bands: it renders with 4x MSAA in three
// predicated tiles of 1280x256, which fill the 10 MiB EDRAM exactly (32 bytes
// per pixel), so any extra width overflows. Next step: render that surface with
// 2x MSAA, or use more tiles.
//
// Reverse engineered from default.xex (title 454107EC, v0.0.0.7):
//   sub_8250B0D0  picks the render size from the video mode: widescreen with a
//                 width >= 1280 gives 1280x720, narrower 1024x576. It stores the
//                 size twice, in 0x82C651E8/EC and 0x82C651F0/F4.
//   sub_82500168  SetResolutionMode(state, mode): 0 = 640x480, 1 = 1024x576,
//                 otherwise 1280x720, into the same globals. Also called later
//                 (e.g. from sub_824E94E8), so the override is reapplied there.

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
REX_EXTERN(__imp__sub_82500168);

namespace {

constexpr uint32_t kRenderSizes[][2] = {{0x82C651E8, 0x82C651EC}, {0x82C651F0, 0x82C651F4}};

uint32_t LoadU32(uint8_t* base, uint32_t addr) {
  return __builtin_bswap32(*reinterpret_cast<uint32_t*>(base + addr));
}

void StoreU32(uint8_t* base, uint32_t addr, uint32_t value) {
  *reinterpret_cast<uint32_t*>(base + addr) = __builtin_bswap32(value);
}

// Replaces the game's 1280x720 render size with the configured one.
void ApplyRenderSize(uint8_t* base) {
  const int32_t width = REXCVAR_GET(render_width);
  const int32_t height = REXCVAR_GET(render_height);
  if (width <= 0 && height <= 0) {
    return;
  }
  for (const auto& size : kRenderSizes) {
    // Only the 1280x720 (HD widescreen) mode is overridden.
    if (LoadU32(base, size[0]) != 1280 || LoadU32(base, size[1]) != 720) {
      return;
    }
  }
  const uint32_t new_width = width > 0 ? uint32_t(width) : 1280;
  const uint32_t new_height = height > 0 ? uint32_t(height) : 720;
  for (const auto& size : kRenderSizes) {
    StoreU32(base, size[0], new_width);
    StoreU32(base, size[1], new_height);
  }
  static bool logged = false;
  if (!logged) {
    REXLOG_INFO("nfsc: render size 1280x720 -> {}x{}", new_width, new_height);
    logged = true;
  }
}

}  // namespace

REX_HOOK_RAW(sub_8250B0D0) {
  __imp__sub_8250B0D0(ctx, base);
  ApplyRenderSize(base);
}

REX_HOOK_RAW(sub_82500168) {
  __imp__sub_82500168(ctx, base);
  ApplyRenderSize(base);
}

// View layout: sub_824FFAB0 fills an object with viewport and tile rectangles
// for the HD layout: x1 = 1280 for full-screen rects, the 4x MSAA tile set
// {0,0,1280,256} {0,256,1280,512} {0,512,1280,720} at +356 (and again at +740),
// a 2-tile set with 368-row tiles at +292. Widen every 1280.
REX_EXTERN(__imp__sub_824FFAB0);
REX_HOOK_RAW(sub_824FFAB0) {
  const uint32_t obj = ctx.r3.u32;
  __imp__sub_824FFAB0(ctx, base);
  const int32_t width = REXCVAR_GET(render_width);
  if (width <= 0 || width == 1280) {
    return;
  }
  int replaced = 0;
  for (uint32_t off = 0; off < 1200; off += 4) {
    const uint32_t v = LoadU32(base, obj + off);
    if (v == 1280) {
      StoreU32(base, obj + off, uint32_t(width));
      ++replaced;
    }
  }
  REXLOG_INFO("nfsc: widened {} view layout values to {}", replaced, width);
}
