#pragma once
#include <cstdint>

// Hysteresis edge tracing
// Input:  thresholded image where:
//         255 = strong edge
//         128 = weak edge
//         0   = non-edge
// Output: final binary edge map
//         255 = confirmed edge
//         0   = suppressed
void hysteresis(const uint8_t* input,
                uint8_t* output,
                int width,
                int height);
