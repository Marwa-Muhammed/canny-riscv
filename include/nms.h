#pragma once
#include <cstdint>
#include <cstddef>

// Quantised direction encoding (matches Sobel stage output)
// 0 → 0°   horizontal gradient → compare left / right
// 1 → 45°  diagonal ↗         → compare top-right / bottom-left
// 2 → 90°  vertical gradient  → compare top / bottom
// 3 → 135° diagonal ↘         → compare top-left / bottom-right

// Generic non-maximum suppression template
// TMag → type of magnitude buffer  (e.g. uint8_t)
// TDir → type of direction buffer  (e.g. uint8_t, values 0-3)
// TOut → type of output buffer     (e.g. uint8_t)
//
// A pixel survives only if its magnitude is STRICTLY greater than
// both neighbours along its gradient direction.
// Border pixels (1-pixel border) are always set to 0.
template<typename TMag, typename TDir, typename TOut>
void nonMaxSuppression(const TMag* magnitude,
                       const TDir* direction,
                       TOut*       output,
                       int         width,
                       int         height);

// Convenience wrapper for the most common case (all uint8_t)
void nms_u8(const uint8_t* magnitude,
            const uint8_t* direction,
            uint8_t*       output,
            int            width,
            int            height);
