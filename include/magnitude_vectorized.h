

#include <cstdint>
#include <cstddef>
#include <riscv_vector.h>
#include <cstdlib>

/**
 * Sobel L1 magnitude (|Gx| + |Gy|), vectorized with RVV.
 *
 * APPROACH (two-pass)
 *
 *   PASS 1 — raw magnitude + running max (vector-valued)
 *     Strip-mine across all n = width*height gradient pixels.
 *     Each iteration:
 *       1. vl = vsetvl(remaining)                  -- ask HW how many lanes
 *       2. load a chunk of Gx (int16_t) into a vector register
 *       3. load the matching chunk of Gy
 *       4. |Gx| = max(Gx, -Gx), |Gy| = max(Gy, -Gy)  -- no dedicated abs intrinsic
 *       5. raw = |Gx| + |Gy|                         -- elementwise add
 *       6. store raw into raw_mag[] at the right offset
 *       7. running_max = elementwise_max(running_max, raw)
 *          -- NOT a scalar yet! This is a full vector register that
 *          -- "rides along" through the whole strip-mine loop. Lane i
 *          -- of running_max holds the max of every raw value that
 *          -- ever landed in lane i across all chunks.
 *     After the loop: ONE vector reduction (vredmaxu) collapses
 *     running_max down to a single scalar — the true global max,
 *     because the final reduction maxes across all lanes too.
 *
 *   PASS 2 — normalize
 *     8. scale = 255 / global_max   (computed in Q8 fixed-point to
 *        avoid float in the vectorized inner loop)
 *     For each chunk of raw_mag[]:
 *       - widen-multiply by scale
 *       - shift down, narrow back to 8-bit
 *       - store into mag[]
 *
 *   EDGE_CASE: if global_max == 0 (a flat/blank image — no gradients
 *   anywhere), there is nothing to normalize against. Skip pass 2
 *   entirely and emit an all-zero output instead of dividing by zero.
 */

// ---------------------------------------------------------------------
// Pass 1: compute raw |Gx|+|Gy| for every pixel, return the global max.
//
//   gx, gy     : input gradient buffers, length n, row-major
//   raw_mag    : output buffer, length n -- caller-allocated scratch
//   n          : number of gradient samples (width * height)
//
// Returns: the global maximum raw magnitude found across the image.
// ---------------------------------------------------------------------
uint16_t sobel_magnitude_raw_rvv(const int16_t* gx,
                                  const int16_t* gy,
                                  uint16_t* raw_mag,
                                  int n);

// ---------------------------------------------------------------------
// Pass 2: rescale raw_mag[] into [0,255] using the global max found
// in Pass 1, writing the final 8-bit magnitude image into mag[].
//
// If global_max == 0, mag[] is filled with zeros (blank-image case)
// and no division/scaling is attempted.
// ---------------------------------------------------------------------
void sobel_magnitude_normalize_rvv(const uint16_t* raw_mag,
                                    uint8_t* mag,
                                    int n,
                                    uint16_t global_max);

// ---------------------------------------------------------------------
// Top-level entry point: runs both passes back to back, owning the
// raw_mag[] scratch buffer internally so the caller only deals with
// gx, gy in and mag out.
//
//   gx, gy : input gradient buffers (int16_t), length width*height
//   mag    : output 8-bit magnitude image, length width*height
// ---------------------------------------------------------------------
void sobel_magnitude_rvv(const int16_t* gx,
                          const int16_t* gy,
                          uint8_t* mag,
                          int width,
                          int height);
