// nfsc - graphics profiles ("original" / "remix") and GPU backend selection.

#include "profiles.h"

#include <array>
#include <string_view>

#include <rex/cvar.h>
#include <rex/logging.h>

REXCVAR_DEFINE_STRING(profile, "original", "NFSC",
                      "Graphics profile: original (faithful to the Xbox 360) or remix "
                      "(higher resolution and sharper filtering). Settings you change "
                      "yourself always override the profile.")
    .allowed({"original", "remix"})
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

REXCVAR_DEFINE_STRING(gpu_backend, "vulkan", "NFSC",
                      "GPU backend for the xenos plugin: vulkan, d3d12 or any")
    .allowed({"vulkan", "d3d12", "any"})
    .lifecycle(rex::cvar::Lifecycle::kRequiresRestart);

namespace nfsc {

namespace {

struct ProfileValue {
  std::string_view cvar;
  std::string_view original;
  std::string_view remix;
};

// "original": what the Xbox 360 shows. Accurate EDRAM emulation via fragment
// shader interlock (Vulkan) or rasterizer ordered views (D3D12), the game's own
// texture filtering, native 720p with the game's 2x MSAA.
// "remix": host render targets (fbo / rtv) instead of the interlocked EDRAM
// path, rendered at 2x (2560x1440) with 16x anisotropic filtering. The
// interlocked path keeps an RTX 5080 at 94-97% in races at 60 fps; host render
// targets need ~68% and looked identical in side-by-side captures of the menu
// and a race start (high-speed effects not compared yet).
constexpr std::array kProfileValues = {
    ProfileValue{"render_target_path_vulkan", "fsi", "fbo"},
    ProfileValue{"render_target_path_d3d12", "rov", "rtv"},
    ProfileValue{"anisotropic_override", "-1", "5"},
    ProfileValue{"draw_resolution_scale_x", "1", "2"},
    ProfileValue{"draw_resolution_scale_y", "1", "2"},
    ProfileValue{"native_2x_msaa", "true", "true"},
    ProfileValue{"swap_post_effect", "none", "none"},
    // AMD CAS sharpening when presenting (needs the SDK built with FidelityFX).
    // At 2x on a 1440p display it only sharpens; the effect is mild.
    ProfileValue{"present_effect", "bilinear", "cas"},
    // The game presents on every vblank and times its simulation by the clock,
    // so the vblank rate is the frame cap: 30 fps like the Xbox 360 (which never
    // reached 60), or 60 fps. Race timers run in real time at 30, 60 and 120.
    ProfileValue{"guest_vblank_rate", "30", "60"},
    // Immediate presentation shows only black on NVIDIA (RTX 5080, layered DXGI
    // present); mailbox and FIFO work.
    ProfileValue{"vulkan_allow_present_mode_immediate", "false", "false"},
    // Not graphics, but a sensible default for both: keyboard controls next to
    // a controller (the input system merges both devices for player 1).
    ProfileValue{"mnk_mode", "true", "true"},
};

}  // namespace

std::string ActiveProfile() {
  return REXCVAR_GET(profile);
}

std::string GpuBackend() {
  return REXCVAR_GET(gpu_backend);
}

bool SetCvarDefault(std::string_view name, std::string_view value) {
  for (auto& entry : rex::cvar::GetRegistry()) {
    if (entry.name != name)
      continue;
    // Leave values the user chose alone. Changing the default itself (not just
    // the value) keeps the settings dialog from saving it into nfsc.toml and
    // makes "reset to default" return to it.
    if (entry.source == rex::cvar::Source::kDefault) {
      entry.default_value = std::string(value);
      entry.setter(value);
    }
    return true;
  }
  REXLOG_WARN("nfsc: cvar '{}' does not exist", name);
  return false;
}

void ApplyProfile() {
  const bool remix = ActiveProfile() == "remix";
  for (const auto& value : kProfileValues) {
    SetCvarDefault(value.cvar, remix ? value.remix : value.original);
  }
  REXLOG_INFO("nfsc: graphics profile '{}', GPU backend '{}'", ActiveProfile(), GpuBackend());
}

}  // namespace nfsc
