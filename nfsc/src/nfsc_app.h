// nfsc - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <rex/logging.h>
#include <rex/rex_app.h>
#include <rex/system/gpu_plugin.h>

#include "profiles.h"

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
