// nfsc - our own settings (scope "nfsc"), and helpers to reach any setting by name.
//
// The SDK keeps every setting in one registry. Each value carries the layer that wrote it:
// default, title (our graphics profile), file (nfsc.toml), environment, command line,
// runtime (PC SETTINGS while playing). A higher layer always wins, so a profile only fills
// in what the player didn't choose.

#pragma once

#include <string>
#include <string_view>
#include <vector>

#include <rex/setting.h>
#include <rex/settings_registry.h>

namespace nfsc {

class NfscSettings final : public rex::SettingOwner {
 public:
  NfscSettings() : SettingOwner("nfsc") {}

  REX_SETTING(std::string, profile, "original",
              "Graphics profile: original (faithful to the Xbox 360) or remix (higher "
              "resolution, follows the monitor). Settings you change yourself always override "
              "the profile.",
              rex::Allowed("original", "remix"), rex::kRestart);
  REX_SETTING(std::string, frame_rate_limit, "30",
              "Frame rate limit: a number, or monitor to follow the monitor's refresh rate",
              rex::kRestart);
  REX_SETTING(bool, skip_intro, false,
              "Skip the logos and intro videos at startup, up to the main menu");
  REX_SETTING(bool, pause_in_background, true,
              "Pause a race and ignore the controller while the window isn't in focus");
  REX_SETTING(std::string, aspect_ratio, "auto",
              "Aspect ratio: auto (the monitor's), 16:9, 21:9, 32:9 or W:H", rex::kRestart);
  REX_SETTING(i32, render_width, 0,
              "Render width; 0 follows aspect_ratio. Wider than 1280 gives more view to the "
              "sides",
              rex::Range{0, 3840}, rex::kRestart);
  REX_SETTING(i32, render_height, 0,
              "Render height; 0 is the game's own 720. Experimental", rex::Range{0, 2160},
              rex::kRestart);
  REX_SETTING(bool, portable, false,
              "Keep savegames, the shader cache and the remembered game path in a \"userdata\" "
              "folder next to nfsc.exe",
              rex::kRestart);
  REX_SETTING(bool, skip_edition_check, false,
              "Start even if default.xex is not the Collector's Edition executable this build "
              "was recompiled from (it will most likely crash)",
              rex::kRestart);
  REX_SETTING(i32, test_crash, 0, "Crash on purpose to test crash reports: 1 access violation, 2 abort",
              rex::Range{0, 2}, rex::kRestart, rex::kNoSave);
  REX_SETTING(bool, log_fps, false,
              "Log the game's frame rate (frames presented) once per second, with the slowest "
              "frame of that second");
};

// The instance the app registers.
NfscSettings& Settings();

// Old names from SDK v0.10 configs (flat names in nfsc.toml), so they keep working.
void AddOldNames(rex::SettingsRegistry& registry);

// The names a setting (full name) had in SDK v0.10 configs, ours and the SDK's.
std::vector<std::string> OldNamesOf(std::string_view name);

rex::SettingBase* FindSetting(std::string_view name);
std::string SettingText(std::string_view name);
// Whether the player chose the value (config file, command line or PC SETTINGS), rather
// than it coming from the default or the graphics profile.
bool ChosenByPlayer(std::string_view name);

}  // namespace nfsc
