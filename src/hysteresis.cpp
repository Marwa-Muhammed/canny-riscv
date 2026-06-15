#include "hysteresis.h"
#include <cstring>

void hysteresis(const uint8_t* input,
                uint8_t* output,
                int width,
                int height) {

    // Copy input to output first
    // We work on output (don't touch the original input).
    memcpy(output, input, width * height);

    bool changed = true;

    // Keep iterating until no more weak pixels get promoted
    while (changed) {
        changed = false;

        for (int y = 1; y < height - 1; y++) {
            for (int x = 1; x < width - 1; x++) {

                // Only look at weak pixels (128)
                if (output[y * width + x] != 128)
                    continue;

                // Check all 8 neighbors for a strong pixel (255)
                bool has_strong_neighbor = false;
                for (int dy = -1; dy <= 1; dy++) {
                    for (int dx = -1; dx <= 1; dx++) {
                        if (output[(y + dy) * width + (x + dx)] == 255) {
                            has_strong_neighbor = true;
                            break;
                        }
                    }
                    if (has_strong_neighbor) break;
                }

                // Promote weak to strong if connected
                if (has_strong_neighbor) {
                    output[y * width + x] = 255;
                    changed = true;
                }
            }
        }
    }

    // Suppress all remaining weak pixels
    for (int i = 0; i < width * height; i++) {
        if (output[i] == 128)
            output[i] = 0;
    }
}
