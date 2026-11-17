// nfsc - our own settings and helpers (see settings.h).

#include "settings.h"

#include <algorithm>
#include <utility>

#include <rex/logging/log_category.h>

namespace nfsc {

namespace {

// Names a v0.10 nfsc.toml or command line used, in the root table. Ours moved into the
// nfsc scope; the SDK's own old names that its registry doesn't know yet are added too.
// An empty new name marks a setting that no longer exists.
constexpr std::pair<const char*, const char*> kOldNames[] = {
    {"profile", "nfsc.profile"},
    {"frame_rate_limit", "nfsc.frame_rate_limit"},
    {"skip_intro", "nfsc.skip_intro"},
    {"pause_in_background", "nfsc.pause_in_background"},
    {"aspect_ratio", "nfsc.aspect_ratio"},
    {"render_width", "nfsc.render_width"},
    {"render_height", "nfsc.render_height"},
    {"portable", "nfsc.portable"},
    {"skip_edition_check", "nfsc.skip_edition_check"},
    {"test_crash", "nfsc.test_crash"},
    {"log_fps", "nfsc.log_fps"},
    {"gpu_backend", "gpu.driver"},
    {"guest_vblank_rate", "ui.video_mode_refresh_rate"},
    {"vsync", "gpu.vsync"},
    {"draw_resolution_scale_x", "gpu.resolution_scale_x"},
    {"draw_resolution_scale_y", "gpu.resolution_scale_y"},
    {"anisotropic_override", "gpu.anisotropic_override"},
    {"render_target_path_vulkan", "gpu.vulkan.render_target_path"},
    {"render_target_path_d3d12", "gpu.d3d12.render_target_path"},
    {"native_2x_msaa", "gpu.xenos.native_2x_msaa"},
    {"swap_post_effect", "gpu.xenos.swap_post_effect"},
    {"clear_memory_page_state", "gpu.xenos.clear_memory_page_state"},
    {"vulkan_allow_present_mode_immediate", "gpu.vulkan.allow_present_mode_immediate"},
    {"vulkan_allow_present_mode_mailbox", "gpu.vulkan.allow_present_mode_mailbox"},
    // AMD FidelityFX (CAS, FSR) and our upload patch are not in SDK v0.11.
    {"present_effect", ""},
    {"present_cas_additional_sharpness", ""},
    {"present_fsr_sharpness_reduction", ""},
    {"shared_memory_hot_pages", ""},
    {"log_swap_rate", ""},
    {"present_movie_aspect", ""},
};

NfscSettings g_settings;

}  // namespace

NfscSettings& Settings() {
  return g_settings;
}

void AddOldNames(rex::SettingsRegistry& registry) {
  for (const auto& [old_name, new_name] : kOldNames) {
    if (!registry.IsAlias(old_name)) {
      registry.AddAlias(old_name, new_name);
    }
  }
}

std::vector<std::string> OldNamesOf(std::string_view name) {
  std::vector<std::string> names;
  for (const auto& [old_name, new_name] : kOldNames) {
    if (name == new_name) {
      names.emplace_back(old_name);
    }
  }
  // The SDK's own old names are the last part of the new one (fullscreen, keybind_a).
  const size_t dot = name.rfind('.');
  if (dot != std::string_view::npos) {
    const std::string last(name.substr(dot + 1));
    if (REX_SETTINGS().IsAlias(last) && std::ranges::find(names, last) == names.end()) {
      names.push_back(last);
    }
  }
  return names;
}

rex::SettingBase* FindSetting(std::string_view name) {
  auto setting = REX_SETTINGS().Find(name);
  return setting ? *setting : nullptr;
}

std::string SettingText(std::string_view name) {
  const rex::SettingBase* setting = FindSetting(name);
  return setting ? setting->GetText() : std::string();
}

bool ChosenByPlayer(std::string_view name) {
  const rex::SettingBase* setting = FindSetting(name);
  return setting && setting->GetLayer() > rex::SettingLayer::kTitle;
}

}  // namespace nfsc
