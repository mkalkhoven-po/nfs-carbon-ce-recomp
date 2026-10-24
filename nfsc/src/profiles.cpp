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
// "remix": same accurate pipeline, rendered at 2x (2560x1440) with 16x
// anisotropic filtering. The interlocked EDRAM path is costly: on an RTX 5080
// the main menu takes ~36% GPU at 1x, ~53% at 2x and ~90% at 3x, so 3x is left
// as a manual option for 4K displays.
constexpr std::array kProfileValues = {
    ProfileValue{"render_target_path_vulkan", "fsi", "fsi"},
    ProfileValue{"render_target_path_d3d12", "rov", "rov"},
    ProfileValue{"anisotropic_override", "-1", "5"},
    ProfileValue{"draw_resolution_scale_x", "1", "2"},
    ProfileValue{"draw_resolution_scale_y", "1", "2"},
    ProfileValue{"native_2x_msaa", "true", "true"},
    ProfileValue{"swap_post_effect", "none", "none"},
    // Immediate presentation shows only black on NVIDIA (RTX 5080, layered DXGI
    // present); mailbox and FIFO work.
    ProfileValue{"vulkan_allow_present_mode_immediate", "false", "false"},
};

}  // namespace

std::string ActiveProfile() {
  return REXCVAR_GET(profile);
}

std::string GpuBackend() {
  return REXCVAR_GET(gpu_backend);
}

void ApplyProfile() {
  const bool remix = ActiveProfile() == "remix";
  auto& registry = rex::cvar::GetRegistry();
  for (const auto& value : kProfileValues) {
    std::string_view wanted = remix ? value.remix : value.original;
    bool found = false;
    for (auto& entry : registry) {
      if (entry.name != value.cvar)
        continue;
      found = true;
      // Leave values the user chose alone. Changing the default itself (not
      // just the value) keeps the settings dialog from saving profile values
      // into nfsc.toml and makes "reset to default" return to the profile.
      if (entry.source == rex::cvar::Source::kDefault) {
        entry.default_value = std::string(wanted);
        entry.setter(wanted);
      }
      break;
    }
    if (!found) {
      REXLOG_WARN("nfsc: profile cvar '{}' does not exist", value.cvar);
    }
  }
  REXLOG_INFO("nfsc: graphics profile '{}', GPU backend '{}'", ActiveProfile(), GpuBackend());
}

}  // namespace nfsc
