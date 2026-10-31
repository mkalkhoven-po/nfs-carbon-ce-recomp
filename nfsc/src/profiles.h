// nfsc - graphics profiles ("original" / "remix") and GPU backend selection.

#pragma once

#include <string>
#include <string_view>

namespace nfsc {

// Applies the active profile's values as the defaults of the cvars it covers.
// Cvars set in nfsc.toml, the environment or on the command line keep their
// value. Must run after the config is loaded and before the GPU is created.
void ApplyProfile();

// Sets a cvar's default value unless the user set it (config, environment,
// command line). Returns false if the cvar does not exist.
bool SetCvarDefault(std::string_view name, std::string_view value);

// The value a profile gives a cvar, or "" if the profiles don't cover it.
std::string ProfileDefault(std::string_view profile, std::string_view cvar);

// Name of the active profile ("original" or "remix").
std::string ActiveProfile();

// GPU backend requested for the xenos plugin ("vulkan", "d3d12" or "any").
std::string GpuBackend();

}  // namespace nfsc
