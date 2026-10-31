// nfsc - the in-game PC SETTINGS window: graphics profile, frame rate limit,
// aspect ratio, display mode and a few advanced options, saved to nfsc.toml.

#pragma once

#include <filesystem>
#include <functional>

namespace rex::ui {
class ImGuiDrawer;
}

namespace nfsc {

// Where Apply saves the settings (nfsc.toml next to the executable).
void SetSettingsConfigPath(std::filesystem::path path);

// Called on the UI thread when an applied setting needs a restart. The handler
// calls StartNewInstance and closes the app.
void SetRestartHandler(std::function<void()> handler);

// Opens the window, or closes it if it is open. UI thread only.
void ToggleSettingsWindow(rex::ui::ImGuiDrawer* drawer);
void OpenSettingsWindow(rex::ui::ImGuiDrawer* drawer);

// Starts nfsc.exe again, with the same command line minus the settings changed
// in the window (those are in nfsc.toml now). The app exits with std::_Exit, so
// this can't wait until then; instead the new process waits for this one in
// WaitForPreviousInstance.
void StartNewInstance();

// In a process started by StartNewInstance: waits until the old one has exited,
// so the two never use the same files. Call first thing at startup.
void WaitForPreviousInstance();

}  // namespace nfsc
