// nfsc - widescreen / ultrawide rendering (aspect_ratio, e.g. 21:9 or 32:9).
//
// A wider render size widens the front buffer, HUD and 3D viewport. The main
// scene's 4x MSAA tiles (1280x256) fill the 10 MiB EDRAM exactly, so wider
// tiles drop to 2x MSAA (see the descriptor hook at the end).
//
// Reverse engineered from default.xex (title 454107EC, v0.0.0.7):
//   sub_8250B0D0  picks the render size from the video mode: widescreen with a
//                 width >= 1280 gives 1280x720, narrower 1024x576. It stores the
//                 size twice, in 0x82C651E8/EC and 0x82C651F0/F4.
//   sub_82500168  SetResolutionMode(state, mode): 0 = 640x480, 1 = 1024x576,
//                 otherwise 1280x720, into the same globals. Also called later
//                 (e.g. from sub_824E94E8), so the override is reapplied there.

#include <cstdint>
#include <cstdio>
#include <string>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>

#include "profiles.h"
#include "widescreen.h"

REXCVAR_DEFINE_STRING(aspect_ratio, "16:9", "NFSC",
                      "Screen aspect ratio for the game's 3D view and HUD, e.g. 16:9 (the game's "
                      "own), 21:9 or 32:9. Wider ratios render at 720 * W/H x 720.")
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_INT32(render_width, 0, "NFSC",
                     "Game render width in pixels (0 = from aspect_ratio)")
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

// Render target descriptors: sub_82505998 registers a 208-byte descriptor
// (+8 width, +12 height, +36 MSAA: 0 none, 1 = 2x, 2 = 4x) in a table at
// 0x82D5B250. The main scene uses 1280x256 tiles with 4x MSAA, which fill the
// 10 MiB EDRAM exactly; wider tiles overflow it. When widening, drop such
// descriptors to 2x MSAA so the same tile layout fits again.
REX_EXTERN(__imp__sub_82505998);
REX_HOOK_RAW(sub_82505998) {
  const uint32_t desc = ctx.r5.u32;
  if (REXCVAR_GET(render_width) > 1280 && desc != 0) {
    constexpr uint64_t kEdramBytes = 10 * 1024 * 1024;
    constexpr uint64_t kBytesPerSample = 8;  // 32-bit color + 32-bit depth
    const uint32_t width = LoadU32(base, desc + 8);
    const uint32_t height = LoadU32(base, desc + 12);
    const uint32_t msaa = LoadU32(base, desc + 36);
    if (msaa == 2 && uint64_t(width) * height * 4 * kBytesPerSample > kEdramBytes) {
      StoreU32(base, desc + 36, 1);
      REXLOG_INFO("nfsc: {}x{} render target 4x -> 2x MSAA to fit EDRAM", width, height);
    }
  }
  __imp__sub_82505998(ctx, base);
}

namespace nfsc {

void ApplyWidescreen() {
  unsigned aspect_x = 16, aspect_y = 9;
  const std::string& aspect = REXCVAR_GET(aspect_ratio);
  if (std::sscanf(aspect.c_str(), "%u:%u", &aspect_x, &aspect_y) != 2 || !aspect_x || !aspect_y) {
    REXLOG_WARN("nfsc: invalid aspect_ratio '{}', using 16:9", aspect);
    aspect_x = 16;
    aspect_y = 9;
  }
  // Width at the game's 720 lines, rounded to a multiple of 8. Narrower than
  // 16:9 is not supported (the game would have to shrink its 1280 layout).
  uint32_t width = (720 * aspect_x / aspect_y + 4) / 8 * 8;
  if (width <= 1280) {
    return;
  }
  // 2x MSAA tiles of 256 lines fit the 10 MiB EDRAM up to 2560 pixels.
  if (width > 2560) {
    REXLOG_WARN("nfsc: aspect_ratio {} needs {} px, limited to 2560", aspect, width);
    width = 2560;
  }
  const std::string width_text = std::to_string(width);
  SetCvarDefault("render_width", width_text);
  // The guest video mode drives the presenter's aspect ratio.
  SetCvarDefault("video_mode_width", std::to_string(REXCVAR_GET(render_width)));
  SetCvarDefault("video_mode_height", "720");
  // Movies are 16:9 and drawn stretched across the wider frame; present them at
  // 16:9 (pillarboxed on a wide screen).
  SetCvarDefault("present_movie_aspect", "16:9");
  REXLOG_INFO("nfsc: aspect ratio {} -> render {}x720", aspect, REXCVAR_GET(render_width));
}

}  // namespace nfsc
