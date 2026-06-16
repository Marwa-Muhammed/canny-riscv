#include "direction.h"
#include <cstdlib>  // abs()

/*
Direction computation is a simplification of atan2(Gy, Gx).

Instead of computing angles, we compare magnitudes of Gx and Gy
to decide the dominant edge direction.

*/

/*
This system represents a classification system that converts (Gx, Gy)
 into 4 discrete bins so NMS knows which neighbors to compare.
*/


// ─────────────────────────────────────────
// Direction: quantized edge orientation
// ─────────────────────────────────────────'


void direction_compute(const int16_t* src_gx,
                       const int16_t* src_gy,
                       uint8_t* dst,  // stores the output direction map
                       int width,
                       int height)
{
    int n = width * height;  // total number of pixels, defined to deal with the image as 1D array


    // Loop over all pixels
    for (int i = 0; i < n; i++)
    {

        // Copy values locally for faster access
        int16_t gx = src_gx[i];
        int16_t gy = src_gy[i];

        // The absolute is used as the decision depends on the gradient strength  only not sign
        int ax = abs(gx);
        int ay = abs(gy);

        // ─────────────────────────────
        // Dominant direction decision
        // ─────────────────────────────

        if (ax > ay)
        {
            // Horizontal gradient dominant → vertical edge
            // NMS compares LEFT and RIGHT neighbors
            dst[i] = 0;
        }
        else if (ay > ax)
        {
             // Vertical gradient dominant → horizontal edge
            // NMS compares TOP and BOTTOM neighbors
            dst[i] = 2;
        }
        else
        {
               // ax == ay exactly (rare true diagonal)
    if (ay > ax * 0.9f)  // vertical
        dst[i] = 2;
    else if (ax > ay * 0.9f)  // horizontal
        dst[i] = 0;
    else
    {
        // real diagonal 
        if ((gx >= 0 && gy >= 0) || (gx < 0 && gy < 0))
            dst[i] = 1;  // 45°
        else
            dst[i] = 3;  // 135°
    }
        }
    }
}
