// nfsc - widescreen / ultrawide rendering.

#pragma once

#include <cstdint>

namespace nfsc {

// Derives render_width, the guest video mode and the movie aspect from
// aspect_ratio (unless set explicitly). Call before the guest starts.
void ApplyWidescreen();

// Presents loading screens at 16:9 when widened. Call once per guest frame.
void UpdatePresentAspect(uint8_t* base);

}  // namespace nfsc
