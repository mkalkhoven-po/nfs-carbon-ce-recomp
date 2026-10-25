// nfsc - "EXIT TO DESKTOP" option at the end of the main menu.

#pragma once

#include <functional>

namespace nfsc {

// Called (on a guest thread) when EXIT TO DESKTOP is selected in the main menu.
void SetQuitHandler(std::function<void()> handler);

}  // namespace nfsc
