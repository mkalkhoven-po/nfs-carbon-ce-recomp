// nfsc - finding the game data (a disc image or an extracted folder) and checking that
// it is the Collector's Edition this build was recompiled from.

#pragma once

#include <filesystem>
#include <string>

namespace rex::filesystem {
class IFileSystem;
}

namespace nfsc {

// Decides where savegames and the shader cache go, as title-layer values of app.user_root
// and app.cache_root: portable mode, otherwise Documents\nfsc and %LOCALAPPDATA%\nfsc\cache.
// Values the player set (nfsc.toml, command line) win. Call before ResolveGamePath.
void ConfigureDataPaths(const std::filesystem::path& exe_dir);

// Sets title.game_root when the player didn't: the path remembered from an earlier start, a
// "game" folder or a single .iso next to the executable, or a disc image / default.xex
// chosen in a file dialog. A path found or chosen this way is remembered in the user data
// folder. Returns false when there is no game data (the dialog was canceled).
bool ResolveGamePath(const std::filesystem::path& exe_dir);

// Returns a message for the player if default.xex is not the Collector's Edition executable
// the game code in this build was recompiled from, or "" when it is.
std::string CheckEdition(rex::filesystem::IFileSystem& file_system);

}  // namespace nfsc
