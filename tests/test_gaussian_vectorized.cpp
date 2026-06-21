// Compares gaussian_blur_rvv() against scalar gaussian_blur() on every pixel
// of the image (border + interior) - but instead of demanding bit-exact
// equality, this measures the PRECISION TRADEOFF introduced by replacing
// true division (sum / 273) with the fixed-point approximation
// (sum * 240) >> 16 in the vectorized normalization step (see Hint 6.4).
//
// Expected result: the two should almost always agree, and where they
// differ, the vectorized result should be AT MOST 1 intensity level lower
// than the scalar result (the fixed-point constant 240 is a truncated-down
// approximation of the true ratio 65536/273 ~= 240.06, so the error is
// small, bounded, and always in the same direction - never the reverse).
//
// Must run under QEMU with v=true -- RVV intrinsics don't execute on host x86.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include "gaussian.h"
#include "gaussian_vectorized.h"

int main() {
    const int width  = 136;
    const int height = 136;
    const int n      = width * height;

    uint8_t* src        = (uint8_t*)aligned_alloc(64, n);
    uint8_t* dst_scalar = (uint8_t*)aligned_alloc(64, n);
    uint8_t* dst_vector = (uint8_t*)aligned_alloc(64, n);

    for (int y = 0; y < height; y++)
        for (int x = 0; x < width; x++)
            src[y * width + x] = (uint8_t)((x * 37 + y * 59 + 17) % 256);

    gaussian_blur(src, dst_scalar, width, height);      // full-image reference (true division)
    gaussian_blur_rvv(src, dst_vector, width, height);  // hybrid RVV + scalar border (fixed-point division)

    // ---- Precision statistics, instead of a simple match/mismatch count ----
    int   differing_pixels   = 0;     // pixels where scalar != vector
    int   max_abs_diff       = 0;     // largest |scalar - vector| seen
    long  sum_abs_diff       = 0;     // running total, for the mean
    int   wrong_direction    = 0;     // count of vector > scalar (should be 0, see derivation above)
    int   printed            = 0;     // how many example diffs we've printed

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;

            int scalar_val = dst_scalar[idx];
            int vector_val = dst_vector[idx];
            int diff       = scalar_val - vector_val;  // expected to be >= 0

            if (diff != 0) {
                differing_pixels++;
                sum_abs_diff += std::abs(diff);
                if (std::abs(diff) > max_abs_diff) max_abs_diff = std::abs(diff);

                // Our fixed-point constant (240) is truncated DOWN from the
                // true ratio (~240.06), so the vectorized result should
                // never exceed the scalar result. If it ever does, that is
                // NOT expected precision loss - it points to a real bug
                // (e.g. a clamp, widening, or accumulation error), so we
                // track it separately from ordinary rounding-down error.
                if (diff < 0) wrong_direction++;

                if (printed < 10) {
                    printf("DIFF at (x=%d, y=%d): scalar=%d vector=%d  (diff=%d)\n",
                           x, y, scalar_val, vector_val, diff);
                    printed++;
                }
            }
        }
    }

    double pct_differing = 100.0 * differing_pixels / n;
    double mean_abs_diff  = differing_pixels > 0
                               ? (double)sum_abs_diff / differing_pixels
                               : 0.0;

    printf("\n--- Precision Tradeoff Report ---\n");
    printf("Total pixels checked       : %d\n", n);
    printf("Pixels differing            : %d (%.2f%%)\n", differing_pixels, pct_differing);
    printf("Max |scalar - vector|       : %d\n", max_abs_diff);
    printf("Mean |diff| (over differing): %.4f\n", mean_abs_diff);
    printf("Pixels where vector > scalar (unexpected direction): %d\n", wrong_direction);

    // ---- Pass/fail criterion for the precision tradeoff ----
    // We are no longer demanding bit-exact equality (differing_pixels == 0)
    // because the fixed-point approximation is EXPECTED to differ from true
    // division sometimes. Instead we pass if:
    //   1. every difference is within the predicted +/-1 bound, AND
    //   2. no difference ever goes in the unexpected direction (vector > scalar)
    // A larger max_abs_diff or any wrong_direction count means something
    // beyond the expected fixed-point rounding error is going on (a real bug),
    // not an acceptable precision tradeoff.
    const int EXPECTED_MAX_DIFF = 1;
    bool within_expected_precision = (max_abs_diff <= EXPECTED_MAX_DIFF) && (wrong_direction == 0);

    free(src); free(dst_scalar); free(dst_vector);

    if (within_expected_precision) {
        printf("PASS (differences are within the expected fixed-point rounding error)\n");
        return 0;
    }
    printf("FAIL (difference exceeds expected fixed-point precision bound, or wrong direction detected)\n");
    return 1;
}
