// nfsc - Need for Speed: Carbon Collector's Edition, recompiled with the ReXGlue SDK.
//
// The app: our settings and graphics profile, the game data, PC SETTINGS (F1), fullscreen
// (F11), pausing a race in the background, and the edition check.

#include <format>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <rex/app/title_app.h>
#include <rex/cli.h>
#include <rex/platform/file.h>
#include <rex/hid/input_system.h>
#include <rex/input_keys.h>
#include <rex/kernel/object/notification_broadcaster.h>
#include <rex/kernel/title_runtime.h>
#include <rex/logging/log_category.h>
#include <rex/path_settings.h>
#include <rex/settings_registry.h>
#include <rex/ui/ui_event.h>
#include <rex/ui/ui_system.h>

#include "generated/default/nfsc_init.h"

#include "crash_report.h"
#include "display.h"
#include "game_path.h"
#include "main_menu.h"
#include "pad.h"
#include "profiles.h"
#include "settings.h"
#include "settings_window.h"
#include "widescreen.h"

namespace {

// The whole command line; App::GetArgs only has the positional arguments.
std::vector<std::string> g_args;

// The notification the Xbox guide sends (XN_SYS_UI). The game pauses a race on it, like it
// must on the console.
constexpr rex::kernel::XNotificationID kSystemUiNotification = 0x9;

// Options the SDK reads without a scope (see the SDK's settings reference).
constexpr std::pair<std::string_view, std::string_view> kPlainOptions[] = {
    {"game_data_root", "title.game_root"},
    {"user_data_root", "app.user_root"},
    {"cache_root", "app.cache_root"},
    {"config_file", "app.config_file"},
};

struct Option {
  std::string name;
  std::string value;
};

// The --name=value, --name value, --name and --no-name options of the command line.
std::vector<Option> ParseOptions(std::span<const std::string> args) {
  std::vector<Option> options;
  for (size_t i = 0; i < args.size(); i++) {
    std::string_view arg = args[i];
    if (!arg.starts_with("--")) {
      continue;
    }
    arg.remove_prefix(2);
    Option option;
    if (const size_t eq = arg.find('='); eq != std::string_view::npos) {
      option = {std::string(arg.substr(0, eq)), std::string(arg.substr(eq + 1))};
    } else if (arg.starts_with("no-")) {
      option = {std::string(arg.substr(3)), "false"};
    } else if (i + 1 < args.size() && !args[i + 1].starts_with("--")) {
      option = {std::string(arg), args[++i]};
    } else {
      option = {std::string(arg), "true"};
    }
    for (const auto& [plain, full] : kPlainOptions) {
      if (option.name == plain) {
        option.name = full;
      }
    }
    options.push_back(std::move(option));
  }
  return options;
}

// A placeholder overlay that only gives a key a setting of its own (ui.bind.<name>): the
// overlay registry calls OnToggle before it would show it, and it never shows.
template <void (*Action)()>
class KeyAction final : public rex::ui::DialogBase {
 public:
  bool Draw(ImGuiIO&) override { return false; }
  bool OnToggle() override {
    Action();
    return true;
  }
  static std::unique_ptr<rex::ui::IDialog> Create(rex::ui::IUISystem&) {
    return std::make_unique<KeyAction>();
  }
};

void ToggleFullscreen() {
  const bool fullscreen = nfsc::SettingText("ui.fullscreen") == "true";
  (void)REX_SETTINGS().SetText(rex::SettingLayer::kRuntime, "ui.fullscreen",
                               fullscreen ? "false" : "true");
}

}  // namespace

class NfscApp : public rex::app::TitleApp, public rex::ui::IUIEventListener {
 public:
  NfscApp() : TitleApp("nfsc", kPPCImageConfig) {}

 protected:
  rex::Result<void> OnConfigure(rex::SettingsRegistry& registry) override {
    const std::filesystem::path exe_dir = rex::platform::GetExecutableFolder();
    nfsc::InstallCrashReport(exe_dir / "logs");
    if (auto result = registry.Register(nfsc::Settings()); !result) {
      return result;
    }
    nfsc::AddOldNames(registry);

    // The profile, the aspect ratio and the monitor decide values that the SDK reads during
    // startup, before OnInit, so the player's own values are needed now. The SDK reads the
    // config file and the command line again later, at the same layers.
    std::vector<Option> options = ParseOptions(g_args);
    for (const auto& option : options) {
      if (option.name == "app.config_file") {
        (void)registry.SetText(rex::SettingLayer::kCommandLine, option.name, option.value);
      }
    }
    const std::filesystem::path config_file = REX_PATHS().config_file.Get();
    std::error_code ec;
    if (std::filesystem::exists(config_file, ec)) {
      (void)registry.Load(config_file, rex::SettingLayer::kFile);
    }
    for (const auto& option : options) {
      (void)registry.SetText(rex::SettingLayer::kCommandLine, option.name, option.value);
    }

    nfsc::ApplyProfile(nfsc::Settings().profile.Get());
    nfsc::ApplyWidescreen();
    ApplyFrameRateLimit(registry);
    nfsc::ConfigureDataPaths(exe_dir);
    if (!nfsc::ResolveGamePath(exe_dir)) {
      rex::app::RequestShutdown(0);  // No disc image or folder chosen.
      return rex::Ok();
    }
    (void)registry.SetText(
        rex::SettingLayer::kTitle, "ui.title",
        std::format("Need for Speed: Carbon Collector's Edition - Recompiled v{} ({})",
                    NFSC_VERSION, nfsc::ActiveProfile() == "remix" ? "Remix" : "Original"));
    nfsc::SetSettingsConfigPath(config_file);
    return rex::Ok();
  }

  rex::platform::CrashReporterOptions GetCrashOptions() override {
    // Our crash report shows its own message, with the dump.
    return {.app_name = "Need for Speed: Carbon", .show_dialog = false};
  }

  rex::Result<void> OnInit() override {
    nfsc::InstallAbortReport();
    nfsc::SetInputSystem(&GetInput());
    nfsc::SetSettingsUI(&GetUI());

    // F11 switches between fullscreen and a window with a title bar (and close button).
    // Alt+F4 or EXIT TO DESKTOP in the main menu exits. F1 or PC SETTINGS in the main menu
    // opens the settings window.
    const rex::ui::OverlayConfig keys[] = {
        {"pc_settings", {rex::VirtualKey::kF1}, KeyAction<nfsc::ToggleSettingsWindow>::Create},
        {"fullscreen", {rex::VirtualKey::kF11}, KeyAction<ToggleFullscreen>::Create},
    };
    if (auto result = GetUI().AddOverlays(keys); !result) {
      REXLOG_WARN("nfsc: F1 and F11 are not bound: {}", result.error().GetDescription());
    }
    listener_ = GetUI().AddEventListener(*this);

    nfsc::SetQuitHandler([] { rex::app::RequestShutdown(0); });
    nfsc::SetSettingsHandler([this] { GetUI().PostTask([] { nfsc::OpenSettingsWindow(); }); });
    nfsc::SetPauseHandler([this](bool open) {
      settings_open_ = open;
      UpdateGuestPause();
    });
    nfsc::SetRestartHandler([this] { rex::app::RequestRestart(ArgsForRestart()); });
    return rex::Ok();
  }

  // Only the executable the game code was recompiled from runs correctly.
  rex::Result<void> OnTitleLaunch(rex::kernel::TitleRuntime& title) override {
    if (const std::string message = nfsc::CheckEdition(title.GetFileSystem());
        !message.empty()) {
      return rex::Err(rex::ErrorCategory::kConfig, message);
    }
    nfsc::TestCrash();
    nfsc::StartIntroSkip();
    return rex::Ok();
  }

  void OnUIEvent(const rex::ui::UIEvent& event) override {
    if (event.GetType() != rex::ui::UIEvent::Type::kFocus) {
      return;
    }
    background_ = !GetUI().IsFocused() && nfsc::Settings().pause_in_background.Get();
    nfsc::SetPadBlockedInBackground(background_);
    UpdateGuestPause();
  }

  void OnInputEvent(rex::InputEvent&) override {}

 private:
  void ApplyFrameRateLimit(rex::SettingsRegistry& registry) {
    // The refresh rate the title sees drives the vblanks the game waits for.
    const double rate = nfsc::FrameRateLimit(nfsc::Settings().frame_rate_limit.Get());
    if (rate >= 24.0 && rate <= 240.0) {
      (void)registry.SetText(rex::SettingLayer::kTitle, "ui.video_mode_refresh_rate",
                             std::format("{}", rate));
    } else if (rate > 0.0) {
      REXLOG_WARN("nfsc: frame rate limit {} is outside 24 to 240", rate);
    }
  }

  // The same command line, without the settings changed in PC SETTINGS, so that nfsc.toml
  // decides those.
  std::vector<std::string> ArgsForRestart() {
    const auto& changed = nfsc::ChangedSettings();
    std::vector<std::string> args;
    const auto& source = g_args;
    for (size_t i = 0; i < source.size(); i++) {
      std::string_view arg = source[i];
      bool drop = false;
      if (arg.starts_with("--")) {
        std::string_view name = arg.substr(2);
        if (name.starts_with("no-")) {
          name.remove_prefix(3);
        }
        name = name.substr(0, name.find('='));
        drop = changed.contains(std::string(name));
        // A value in the next argument goes too.
        if (drop && arg.find('=') == std::string_view::npos && i + 1 < source.size() &&
            !source[i + 1].starts_with("--")) {
          i++;
        }
      }
      if (!drop) {
        args.emplace_back(arg);
      }
    }
    return args;
  }

  void UpdateGuestPause() {
    const bool paused = settings_open_ || background_;
    auto& title = GetTitleRuntime();
    if (paused != guest_paused_ && title.IsRunning()) {
      guest_paused_ = paused;
      title.GetNotificationBroadcaster().BroadcastNotification(kSystemUiNotification, paused);
    }
  }

  rex::ScopeGuard listener_;
  bool settings_open_ = false;
  bool background_ = false;
  bool guest_paused_ = false;
};

// REX_TITLE_APP, keeping the command line.
int NfscMain(NfscApp& app, const rex::CLIArgList& args) {
  g_args.assign(args.GetAll().begin(), args.GetAll().end());
  if (app.Init(args) && app.LaunchTitle()) {
    app.Run();
  }
  return app.Shutdown();
}

REX_TITLE_APP_MAIN(NfscApp, NfscMain)
