#include "magnitude.h"
#include <cmath>
#include <cstdlib>





/*
L2 computes the true Euclidean gradient magnitude and is used by the original Canny algorithm.
 L1 is a computationally cheaper approximation that avoids squaring and square-root operations. 
 It is faster but less accurate because it tends to overestimate diagonal edge strengths. 
 For comparison and performance evaluation, both methods were implemented.
*/



// ─────────────────────────────────────────
// L1 Magnitude: |Gx| + |Gy|
// Fast integer-only method
// ─────────────────────────────────────────
void magnitude_l1(const int16_t* src_gx,
                  const int16_t* src_gy,
                  uint8_t* dst,
                  int width,
                  int height) {

    int n = width * height;  // calculate the total numbner of pixels

    // allocate a temporary array to store magnitudes
    // use int32_t to be safe and avoid any overflow
    int32_t* mag_raw = new int32_t[n];    // we may use vector instead of dynamic allocation for further optimization 
                                         // to avoid stdlib overhead

    // Pass 1: compute raw magnitudes and find maximum to normalize according to maximum value
    int32_t max_val = 0;

    for (int i = 0; i < n; i++) {
        // |Gx| + |Gy|
        mag_raw[i] = abs(src_gx[i]) + abs(src_gy[i]);

        // compare each term with the perceding terms to find the maximum value
        if (mag_raw[i] > max_val)
            max_val = mag_raw[i];
    }

    // Pass 2: normalize to [0, 255] 
    for (int i = 0; i < n; i++) {

        if (max_val > 0)  // to avoid any division by zero in case all pixels are black
            // Scale: (value / max) × 255
            dst[i] = (uint8_t)(mag_raw[i] * 255 / max_val);
        else
            dst[i] = 0;  // avoid division by zero
    }

    delete[] mag_raw;  // release the allocated array from memory, all the needed data after normalization is stored in dst[]
}

// ─────────────────────────────────────────
// L2 Magnitude: sqrt(Gx² + Gy²)
// More accurate but uses floating point
// ─────────────────────────────────────────
void magnitude_l2(const int16_t* src_gx,
                  const int16_t* src_gy,
                  uint8_t* dst,
                  int width,
                  int height) {

    int n = width * height;

    // Temporary array to store raw magnitudes
    // float because sqrt returns float
    float* raw = new float[n];

    // Pass 1: compute raw magnitudes and find maximum ──
    float max_val = 0.0f;

    for (int i = 0; i < n; i++) {
        // sqrt(Gx² + Gy²)
        raw[i] = sqrtf((float)src_gx[i] * src_gx[i] +
                       (float)src_gy[i] * src_gy[i]);

        // Track maximum value
        if (raw[i] > max_val)
            max_val = raw[i];
    }

    // Pass 2: normalize to [0, 255] 
    for (int i = 0; i < n; i++) {
        if (max_val > 0.0f)
            // Scale: (value / max) × 255
            dst[i] = (uint8_t)(raw[i] * 255.0f / max_val);
        else
            dst[i] = 0;  // avoid division by zero
    }

    delete[] raw;
}