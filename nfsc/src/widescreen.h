// nfsc - widescreen / ultrawide rendering.

#pragma once

namespace nfsc {

// Derives render_width, the guest video mode and the movie aspect from
// aspect_ratio (unless set explicitly). Call before the guest starts.
void ApplyWidescreen();

}  // namespace nfsc
