// nfsc - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <cstdlib>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/system/gpu_plugin.h>
#include <rex/ui/keybinds.h>

#include "exit_option.h"
#include "game_path.h"
#include "profiles.h"
#include "widescreen.h"

class NfscApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<NfscApp>(new NfscApp(ctx, "nfsc",
        PPCImageConfig));
  }

  // Loads the Xenos GPU plugin (unless --gpu_plugin says otherwise) with the
  // backend from --gpu_backend, then applies the graphics profile. The order
  // matters: the GPU cvars only exist once the plugin is loaded, and the GPU
  // system reads them later, during its setup.
  void OnPreSetup(rex::RuntimeConfig& config) override {
    if (config.gpu_plugin.empty()) {
      config.gpu_plugin = "xenos";
    }
    if (!config.graphics) {
      std::string backend = nfsc::GpuBackend();
      config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, backend);
      if (!config.graphics && backend != "any") {
        REXLOG_WARN("nfsc: GPU backend '{}' unavailable, falling back", backend);
        config.graphics = rex::system::LoadGpuPlugin(config.gpu_plugin, "any");
      }
    }
    nfsc::ApplyProfile();
    nfsc::ApplyWidescreen();
  }

  // F11 switches between fullscreen and a window with a title bar (and close
  // button). Alt+F4 or EXIT TO DESKTOP in the main menu exits.
  void OnPostSetup() override {
    // Same path as closing the window: terminates the title and exits.
    nfsc::SetQuitHandler([this] {
      app_context().CallInUIThread([this] { window()->RequestClose(); });
    });
    rex::ui::RegisterBind("bind_fullscreen", "F11", "Toggle fullscreen", [] {
      bool fullscreen = rex::cvar::Query<bool>("fullscreen");
      rex::cvar::SetFlagByName("fullscreen", fullscreen ? "false" : "true");
    });
  }

  // After the SDK has set its own title (in SetupPresentation).
  void OnPreLaunchModule() override {
    window()->SetTitle(fmt::format(
        "Need for Speed: Carbon Collector's Edition - Recompiled v{} ({})", NFSC_VERSION,
        nfsc::ActiveProfile() == "remix" ? "Remix" : "Original"));
  }

  // The game data: a disc image or an extracted folder, remembered after the
  // first start (see game_path.h).
  void OnConfigurePaths(rex::PathConfig& paths) override {
    nfsc::ResolveGamePath(paths, rex::filesystem::GetExecutableFolder());
    if (paths.game_data_root.empty()) {
      std::quick_exit(0);  // No disc image or folder chosen.
    }
  }

  // Only the executable the game code was recompiled from runs correctly.
  void OnPostLoadXexImage() override {
    if (!nfsc::CheckEdition(*runtime())) {
      std::quick_exit(1);
    }
  }

  // Override virtual hooks for customization:
  // void OnPostInitLogging() override {}
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnPostSetup() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;
  // void OnShutdown() override {}
  // void OnConfigurePaths(rex::PathConfig& paths) override {}
};
