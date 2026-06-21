// Compares sobel_magnitude_rvv() against scalar magnitude_L1() on every pixel
// of the magnitude image - but instead of demanding bit-exact equality, this
// measures the PRECISION TRADEOFF introduced by replacing the true
// normalization (raw_magnitude * 255 / global_max, a runtime division by a
// data-dependent value) with a fixed-point multiply/shift approximation
// against the __riscv_vredmax-computed global max (see sections 6.5 / 6.4's
// fixed-point approach applied to the reduction result).
//
// Must run under QEMU with v=true -- RVV intrinsics don't execute on host x86.
#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cmath>
#include "magnitude.h"
#include "magnitude_vectorized.h"

// This is a starting assumption, not a derived guarantee.
// Confirm against the actual shift amount in sobel_magnitude_rvv and adjust
// if needed.
static constexpr int EXPECTED_MAX_DIFF = 1;

int main() {
    const int width  = 136;
    const int height = 136;
    const int n       = width * height;

    int16_t* gx = (int16_t*)aligned_alloc(64, n * sizeof(int16_t));
    int16_t* gy = (int16_t*)aligned_alloc(64, n * sizeof(int16_t));

    uint8_t* mag_scalar = (uint8_t*)aligned_alloc(64, n);
    uint8_t* mag_vector = (uint8_t*)aligned_alloc(64, n);

    // Deterministic pseudo-gradient pattern, including negative values
    // and zero, to exercise the abs() logic and the blank-image edge case
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;
            gx[idx] = (int16_t)(((x * 37 + y * 59) % 201) - 100); // range ~[-100,100]
            gy[idx] = (int16_t)(((x * 71 + y * 13) % 161) - 80);  // range ~[-80,80]
        }
    }

    magnitude_l1(gx, gy, mag_scalar, width, height);        // full-image reference (true division by global_max)
    sobel_magnitude_rvv(gx, gy, mag_vector, width, height); // RVV: fixed-point multiply/shift against vredmax result

    // ---- Precision statistics, instead of a simple match/mismatch count ----
    int   differing_pixels   = 0;   // pixels where scalar != vector
    int   max_abs_diff       = 0;   // largest |scalar - vector| seen
    long  sum_abs_diff       = 0;   // running total, for the mean
    int   wrong_direction    = 0;   // count of vector > scalar (unexpected, see note below)
    int   printed            = 0;   // how many example diffs we've printed

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {
            int idx = y * width + x;

            int scalar_val = mag_scalar[idx];
            int vector_val = mag_vector[idx];
            int diff       = scalar_val - vector_val;  // expected to be >= 0, same direction as Gaussian's case

            if (diff != 0) {
                differing_pixels++;
                sum_abs_diff += std::abs(diff);
                if (std::abs(diff) > max_abs_diff) max_abs_diff = std::abs(diff);

                // As with Gaussian, IF the fixed-point scale constant is
                // truncated down from the true reciprocal, the vectorized
                // result should never exceed the scalar result. Tracked
                // separately because this assumption is carried over from
                // Gaussian, not independently confirmed for this kernel -
                // any count here is worth investigating regardless of
                // EXPECTED_MAX_DIFF.
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

    printf("\n--- Precision Tradeoff Report (Sobel L1 Magnitude) ---\n");
    printf("Total pixels checked       : %d\n", n);
    printf("Pixels differing            : %d (%.2f%%)\n", differing_pixels, pct_differing);
    printf("Max |scalar - vector|       : %d\n", max_abs_diff);
    printf("Mean |diff| (over differing): %.4f\n", mean_abs_diff);
    printf("Pixels where vector > scalar (unexpected direction): %d\n", wrong_direction);
    printf("Expected max diff (configured, see header comment): %d\n", EXPECTED_MAX_DIFF);

    // ---- Pass/fail criterion for the precision tradeoff ----
    // Same logic as the Gaussian test: pass if every difference is within
    // the expected fixed-point rounding bound AND no difference goes in the
    // unexpected direction. Because EXPECTED_MAX_DIFF here is a carried-over
    // assumption rather than a derived guarantee (see header comment), a
    // failure on this bound specifically (rather than on wrong_direction)
    // is a signal to go check the actual shift amount in
    // sobel_magnitude_rvv's normalization step before assuming it's a bug.
    bool within_expected_precision = (max_abs_diff <= EXPECTED_MAX_DIFF) && (wrong_direction == 0);

    free(gx); free(gy); free(mag_scalar); free(mag_vector);

    if (within_expected_precision) {
        printf("PASS (differences are within the expected fixed-point rounding error)\n");
        return 0;
    }
    printf("FAIL (difference exceeds expected fixed-point precision bound, or wrong direction detected)\n");
    return 1;
}
