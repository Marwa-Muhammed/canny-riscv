#include "double_threshold.h"

void double_threshold(const uint8_t* magnitude,
                      uint8_t*       output,
                      int            width,
                      int            height,
                      uint8_t        low_thresh,
                      uint8_t        high_thresh)
{
    // Total number of pixels - process the entire image as a flat 1D array.
    // This is valid because pixels are stored row by row contiguously in memory.
    int total_pixels = width * height;

    for (int i = 0; i < total_pixels; i++) {
        uint8_t val = magnitude[i];

        if (val >= high_thresh) {
            // Strong edge: magnitude is high enough to be a definite edge.
            // Hysteresis will keep these unconditionally.
            output[i] = STRONG_EDGE;

        } else if (val >= low_thresh) {
            // Weak edge: magnitude is in the uncertain range.
            // Hysteresis will keep these only if connected to a strong edge.
            output[i] = WEAK_EDGE;

        } else {
            // Not an edge: magnitude is too low, discard completely.
            output[i] = NO_EDGE;
        }
    }
}