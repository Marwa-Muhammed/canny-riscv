#pragma once
#include <cstdint>

/*
 * gaussian_padded.h
 * =================
 * Declares the padded variant of Gaussian blur for Phase 4 analysis.
 *
 * WHY THIS EXISTS (Project "Deeper Idea"):
 * ----------------------------------------
 * The original gaussian_blur() uses a boundary check inside the inner loop:
 *
 *   if (ir >= 0 && ir < height && ic >= 0 && ic < width)
 *       pixel = src[ir * width + ic];
 *
 * This conditional prevents the compiler from auto-vectorizing the loop
 * because vectorizers cannot handle control flow (branches) inside loops.
 * The -fopt-info-vec-all report confirms:
 *   "not vectorized: unsupported use in stmt"
 *
 * SOLUTION: Pre-pad the image with zeros BEFORE the convolution loop,
 * so the inner loop can read from the padded buffer WITHOUT any boundary
 * check. The zero-padding produces identical results to zero-padding done
 * via the conditional, but the loop structure is now branch-free and
 * potentially vectorizable.
 *
 * This is the standard technique used in production vision libraries
 * (OpenCV, libjpeg-turbo) to enable SIMD vectorization of convolutions.
 *
 * SCRATCH BUFFER DESIGN:
 * ----------------------
 * The padded buffer is passed in by the caller (scratch) instead of being
 * allocated inside this function. This is critical for benchmarking:
 * allocating and zero-initialising (width+4)*(height+4) bytes on every call
 * would dominate the timing at 150+ iterations, measuring malloc overhead
 * instead of convolution performance.
 *
 * The caller allocates scratch ONCE with:
 *   int pw = width + 4, ph = height + 4;
 *   uint8_t* scratch = (uint8_t*)aligned_alloc(64, pw * ph);
 *   memset(scratch, 0, pw * ph);   // zero once — border stays 0 forever
 *
 * gaussian_blur_padded() never writes to the border area of scratch, so
 * the single upfront memset is sufficient across all benchmark iterations.
 */

// Required scratch buffer size in bytes: (width + 4) * (height + 4)
// Caller must allocate this with aligned_alloc(64, ...) and memset to 0
// before the first call. No re-zeroing is needed between iterations.
inline int gaussian_padded_scratch_size(int width, int height) {
    return (width + 4) * (height + 4);
}

// Padded Gaussian blur — no boundary check in inner loop.
// Produces identical output to gaussian_blur() but with a branch-free
// inner loop structure that the compiler can potentially auto-vectorize.
//
// Parameters:
//   src     → input image (uint8_t, width × height, row-major)
//   dst     → output image (uint8_t, must be pre-allocated, width × height)
//   scratch → caller-allocated zero-initialised buffer of size
//             (width+4)*(height+4) bytes. Must be aligned to 64 bytes.
//             Only needs to be zeroed ONCE before the first call.
//   width   → image width in pixels
//   height  → image height in pixels
void gaussian_blur_padded(const uint8_t* src,
                           uint8_t*       dst,
                           uint8_t*       scratch,
                           int            width,
                           int            height);
