// nfsc - graphics profiles ("original" / "remix") and GPU backend selection.

#pragma once

#include <string>

namespace nfsc {

// Applies the active profile's values as the defaults of the cvars it covers.
// Cvars set in nfsc.toml, the environment or on the command line keep their
// value. Must run after the config is loaded and before the GPU is created.
void ApplyProfile();

// Name of the active profile ("original" or "remix").
std::string ActiveProfile();

// GPU backend requested for the xenos plugin ("vulkan", "d3d12" or "any").
std::string GpuBackend();

}  // namespace nfsc
