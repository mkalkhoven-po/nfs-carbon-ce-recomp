// nfsc - graphics profiles ("original" / "remix").
//
// A profile is a set of setting values written at the SDK's title layer, below the config
// file, the command line and PC SETTINGS, so anything the player sets wins.

#pragma once

#include <string>
#include <string_view>

namespace nfsc {

// Writes the profile's values at the title layer. Call from App::OnConfigure.
void ApplyProfile(std::string_view profile);

// The profile's value for a setting (full name), or "" when the profile doesn't set it.
std::string ProfileDefault(std::string_view profile, std::string_view setting);

// A frame rate limit as a number, with "monitor" resolved; 0 when it isn't valid.
double FrameRateLimit(std::string_view limit);

// The profile of this run ("original" or "remix").
std::string ActiveProfile();

}  // namespace nfsc
