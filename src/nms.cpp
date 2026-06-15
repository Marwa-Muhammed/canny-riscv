#include "nms.h"

// Generic non-maximum suppression with 4-direction neighbour comparison.
// Zero-padding approach: border pixels are set to 0 (safe, consistent
// with the Gaussian stage which also zero-pads its border).
//
// TMag → type of magnitude buffer  (uint8_t or uint16_t)
// TDir → type of direction buffer  (uint8_t, values 0-3)
// TOut → type of output buffer     (uint8_t)
template<typename TMag, typename TDir, typename TOut>
void nonMaxSuppression(const TMag* magnitude,
                       const TDir* direction,
                       TOut*       output,
                       int         width,
                       int         height)
{
    // Zero the entire output first (handles border pixels automatically)
    for (int i = 0; i < width * height; i++)
        output[i] = static_cast<TOut>(0);

    // Process interior pixels only (skip 1-pixel border)
    for (int y = 1; y < height - 1; y++) {
        for (int x = 1; x < width - 1; x++) {

            int   idx = y * width + x;
            TMag  mag = magnitude[idx];
            TDir  dir = direction[idx];

            TMag n1, n2;  // the two neighbours along the gradient direction

            // Select neighbours based on quantised direction.
            // dir == 0 → gradient is horizontal → edge is vertical
            //            compare pixel to its LEFT and RIGHT
            // dir == 1 → gradient is diagonal ↗
            //            compare to TOP-RIGHT (y-1,x+1) and BOTTOM-LEFT (y+1,x-1)
            // dir == 2 → gradient is vertical → edge is horizontal
            //            compare pixel to its TOP and BOTTOM
            // dir == 3 → gradient is diagonal ↘
            //            compare to TOP-LEFT (y-1,x-1) and BOTTOM-RIGHT (y+1,x+1)
            switch (static_cast<int>(dir)) {
                case 0:
                    n1 = magnitude[y * width + (x - 1)];
                    n2 = magnitude[y * width + (x + 1)];
                    break;
                case 1:
                    n1 = magnitude[(y - 1) * width + (x + 1)];
                    n2 = magnitude[(y + 1) * width + (x - 1)];
                    break;
                case 2:
                    n1 = magnitude[(y - 1) * width + x];
                    n2 = magnitude[(y + 1) * width + x];
                    break;
                case 3:
                    n1 = magnitude[(y - 1) * width + (x - 1)];
                    n2 = magnitude[(y + 1) * width + (x + 1)];
                    break;

                   default:
                  continue;
             }

            // Keep pixel only if it is STRICTLY greater than both neighbours.
            // Using >= would keep plateaus (flat ridges) → thick edges.
            // Strict > thins edges to 1-pixel width.
            if (mag > n1 && mag > n2)
                output[idx] = static_cast<TOut>(mag);
            // else output[idx] stays 0 from initialisation above
        }
    }
}

// Convenience wrapper: explicit instantiation for uint8_t pipeline
void nms_u8(const uint8_t* magnitude,
            const uint8_t* direction,
            uint8_t*       output,
            int            width,
            int            height)
{
    nonMaxSuppression<uint8_t, uint8_t, uint8_t>(
        magnitude, direction, output, width, height);
}

// Explicit template instantiations (avoids linker errors)

template void nonMaxSuppression<uint16_t, uint8_t, uint8_t>(
    const uint16_t*, const uint8_t*, uint8_t*, int, int);
