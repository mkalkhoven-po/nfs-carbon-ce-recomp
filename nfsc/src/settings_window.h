// nfsc - the in-game PC SETTINGS window: graphics profile, frame rate limit, aspect ratio,
// display mode, intro videos, keyboard controls and a few advanced options. Opened from the
// main menu (PC SETTINGS) or with F1 (ui.bind.pc_settings). Apply saves the choices to
// nfsc.toml; settings that need a restart restart the game.

#pragma once

#include <filesystem>
#include <functional>
#include <set>
#include <string>

namespace rex::ui {
class IUISystem;
}

namespace nfsc {

// The UI system the window opens in; set by the app once the window is open.
void SetSettingsUI(rex::ui::IUISystem* ui);

// The config file Apply writes to (app.config_file).
void SetSettingsConfigPath(std::filesystem::path path);

// Called after Apply saved a change that needs a restart.
void SetRestartHandler(std::function<void()> handler);

// Called with true when the window opens and false when it closes.
void SetPauseHandler(std::function<void(bool)> handler);

void OpenSettingsWindow();
void ToggleSettingsWindow();

// Full names of the settings changed in the window this session, with their old names.
// The restart leaves these off the command line, so that nfsc.toml decides.
const std::set<std::string>& ChangedSettings();

}  // namespace nfsc
