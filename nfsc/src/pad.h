// nfsc - the game's controller reads: taking the pad away from the game for the
// PC SETTINGS window and while in the background, and skipping the intros.

#pragma once

#include <cstdint>

namespace nfsc {

// While captured, the game sees an untouched pad. After release it gets the pad
// back once everything is let go, so the button that closed the window doesn't
// reach it.
void SetPadCaptured(bool captured);

// The buttons (X_INPUT_GAMEPAD_*), with both sticks folded into the d-pad: the
// keyboard's WASD and arrow keys are the two sticks.
uint32_t ReadPad();

// Keeps the pad from the game while the window is in the background.
void SetPadBlockedInBackground(bool blocked);

// With skip_intro: holds Start in pulses until StopIntroSkip, which is what
// skipping the videos and logos by hand comes down to.
void StartIntroSkip();
void StopIntroSkip();

}  // namespace nfsc
