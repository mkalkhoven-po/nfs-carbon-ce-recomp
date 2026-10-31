// nfsc - finding the game data (a disc image or an extracted folder) and checking that
// it is the Collector's Edition this build was recompiled from.

#pragma once

#include <filesystem>

#include <rex/rex_app.h>

namespace rex {
class Runtime;
}

namespace nfsc {

// Sets paths.game_data_root when it wasn't configured (command line, nfsc.toml):
// the path remembered from an earlier start, a "game" folder or a single .iso next
// to the executable, or a disc image / default.xex chosen in a file dialog. A path
// found or chosen this way is remembered in the user data folder.
void ResolveGamePath(rex::PathConfig& paths, const std::filesystem::path& exe_dir);

// Returns false, after telling the user, if the loaded default.xex is not the
// Collector's Edition executable the game code in this build was recompiled from.
bool CheckEdition(rex::Runtime& runtime);

}  // namespace nfsc
