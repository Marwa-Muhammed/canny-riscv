#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
// Custom headers for each stage of the Canny edge detection pipeline
#include "image_io.h"          // Image loading/saving utilities
#include "gaussian.h"          // Stage 1: Gaussian blur (noise reduction)
#include "sobel.h"             // Stage 2a: Sobel gradient filter (edge detection)
#include "magnitude.h"         // Stage 2b: Gradient magnitude (L1 or L2 norm)
#include "direction.h"         // Stage 2c: Gradient direction (angle quantization)
#include "nms.h"               // Stage 3: Non-Maximum Suppression (edge thinning)
#include "double_threshold.h"  // Stage 4: Hysteresis double thresholding
#include "hysteresis.h"        // Stage 5: Hysteresis edge tracking
#include "embedded_image.h"    // Compile-time embedded grayscale image data + dimensions

// ===================================================================
// TIMING SYSTEM
// ===================================================================
// Platform-specific high-resolution timer abstraction.
// On RISC-V (e.g. running under QEMU): uses clock_gettime with
//   CLOCK_MONOTONIC, which reads stable wall-clock nanoseconds.
// On x86 host: same POSIX clock_gettime, but with compiler memory
//   barriers (asm volatile) to prevent the optimizer from reordering
//   timing calls across the measured code.
// Both paths return a uint64_t timestamp in nanoseconds.
// ===================================================================

#ifdef __riscv
// ------------------- RISC-V Timing -------------------
#include <time.h>

// CLOCK_MONOTONIC may not be defined in all bare-metal/QEMU toolchains,
// so define it manually as 1 (its standard POSIX value) if missing.
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif

// Forward-declare clock_gettime so the linker resolves it from libc
// even if the RISC-V sysroot doesn't expose it in the usual headers.
extern "C" int clock_gettime(int clk_id, struct timespec* tp);

// Returns the current wall-clock time in nanoseconds.
// Combines tv_sec (seconds) and tv_nsec (nanoseconds) into a single
// monotonically increasing 64-bit nanosecond timestamp.
static inline uint64_t read_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
const char* timing_name = "clock_gettime (real wall-clock, ns)";

#else
// ------------------- Host (x86) Timing -------------------
#include <time.h>

// Returns current monotonic time in nanoseconds.
// The two asm volatile("" ::: "memory") barriers act as compiler fences:
// they prevent the compiler from hoisting or sinking code across the
// clock_gettime calls, ensuring the measured region is exactly `call`.
static inline uint64_t read_time() {
    struct timespec ts;
    asm volatile("" ::: "memory");   // Compiler barrier: flush all pending reads/writes
    clock_gettime(CLOCK_MONOTONIC, &ts);
    asm volatile("" ::: "memory");   // Compiler barrier: prevent reordering after read
    return (uint64_t)ts.tv_sec * 1000000000ULL + ts.tv_nsec;
}
const char* timing_name = "clock_gettime";
#endif

// ===================================================================
// BENCHMARK MACRO
// ===================================================================
// BENCH(label, call) times a single expression `call` and prints
// the average execution time in nanoseconds over ITERATIONS runs.
//
// Structure:
//   1. Warmup (5 runs): primes instruction caches, branch predictors,
//      and any lazy initialization inside `call` so the timed runs
//      reflect steady-state performance, not cold-start overhead.
//   2. Timed loop (ITERATIONS runs): accumulates total nanoseconds,
//      then divides to get a stable average. Averaging is especially
//      important under QEMU where individual measurements can be noisy.
//   3. The asm volatile("" ::: "memory") inside the loop is a compiler
//      fence that prevents `call` from being optimized away (dead-code
//      elimination) or merged across iterations.
// ===================================================================
#define ITERATIONS 100  // Number of timed repetitions per benchmark

#define BENCH(label, call)                                                      \
do {                                                                            \
    /* --- Warmup phase: not timed, just primes caches/predictors --- */        \
    for (int _i = 0; _i < 5; _i++) {                                           \
        call;                                                                   \
        asm volatile("" ::: "memory"); /* prevent call from being removed */   \
    }                                                                           \
    /* --- Timed phase: measure ITERATIONS runs and sum nanoseconds --- */      \
    uint64_t total = 0;                                                         \
    for (int _i = 0; _i < ITERATIONS; _i++) {                                  \
        uint64_t _c0 = read_time();          /* timestamp before call */        \
        call;                                                                   \
        asm volatile("" ::: "memory");       /* compiler fence */               \
        uint64_t _c1 = read_time();          /* timestamp after call */         \
        total += (_c1 - _c0);               /* accumulate elapsed ns */         \
    }                                                                           \
    /* Compute and print the per-call average */                                \
    double avg_ns = (double)total / ITERATIONS;                                 \
    printf(" %-25s : %.1f %s (avg over %d runs)\n",                            \
           label, avg_ns, timing_name, ITERATIONS);                             \
} while(0)

// ===================================================================
// HEX DUMP SUPPORT (debugging only)
// ===================================================================
// When SHOW_HEX_DUMPS is set to 1, each pipeline stage's output buffer
// is printed as a continuous hex string. A companion Python script
// (decoder_dump.py) can parse these markers and reconstruct .pgm images
// for visual inspection of intermediate results.
//
// Set to 0 (default) for normal benchmark runs — hex output is large
// and would flood the console / slow down timing.
// ===================================================================
#define SHOW_HEX_DUMPS 0

#if SHOW_HEX_DUMPS
// Prints a labeled hex dump of a grayscale uint8 image buffer.
// Output format is understood by decoder_dump.py.
// Each line holds 32 bytes (64 hex chars) for readability.
static void dump_image_hex(const char* stage_name, const uint8_t* buffer,
                           int width, int height) {
    // Header marker: decoder uses name/width/height to reconstruct the image
    printf("=== DUMP_START name=%s width=%d height=%d ===\n",
           stage_name, width, height);
    int total = width * height;
    for (int i = 0; i < total; i++) {
        printf("%02x", buffer[i]);          // Print each byte as 2 hex digits
        if ((i + 1) % 32 == 0) printf("\n"); // Newline every 32 bytes
    }
    if (total % 32 != 0) printf("\n"); // Flush any incomplete final line
    printf("=== DUMP_END ===\n");      // Footer marker for the decoder
}
#endif

// ===================================================================
// run_pipeline — Complete Canny Edge Detection Pipeline
// ===================================================================
// Allocates intermediate buffers, then runs (and benchmarks) all 5
// stages of Canny edge detection in sequence:
//
//   src (input grayscale image)
//    │
//    ▼  Stage 1  – Gaussian Blur          → blurred
//    ▼  Stage 2a – Sobel Gradient         → Gx, Gy
//    ▼  Stage 2b – Gradient Magnitude     → mag   (L1 or L2 depending on use_l2)
//    ▼  Stage 2c – Gradient Direction     → dir
//    ▼  Stage 3  – Non-Maximum Suppression→ nms_out
//    ▼  Stage 4  – Double Threshold       → thresh_out
//    ▼  Stage 5  – Hysteresis             → final_out
//
// Parameters:
//   src     – pointer to the input grayscale image (width × height bytes)
//   width   – image width in pixels
//   height  – image height in pixels
//   use_l2  – if true, use L2 norm √(Gx²+Gy²); otherwise L1 |Gx|+|Gy|
//   label   – human-readable pipeline name printed in benchmark output
//   prefix  – short prefix used to name hex dump stages (debug only)
// ===================================================================
static void run_pipeline(const uint8_t* src, int width, int height,
                         bool use_l2, const char* label, const char* prefix) {
    (void)prefix;  // Suppress "unused variable" warning when SHOW_HEX_DUMPS=0

    const int n = width * height;  // Total number of pixels

    // Declare all intermediate buffer pointers (initialized to nullptr
    // so the cleanup label can safely call free() even on partial allocation).
    uint8_t*  blurred    = nullptr; // Output of Gaussian blur (uint8, grayscale)
    int16_t*  Gx         = nullptr; // Horizontal Sobel gradient (signed 16-bit)
    int16_t*  Gy         = nullptr; // Vertical Sobel gradient   (signed 16-bit)
    uint8_t*  mag        = nullptr; // Gradient magnitude (normalized to 0–255)
    uint8_t*  dir        = nullptr; // Quantized gradient direction (0/1/2/3 → 0°/45°/90°/135°)
    uint8_t*  nms_out    = nullptr; // After non-maximum suppression (thinned edges)
    uint8_t*  thresh_out = nullptr; // After double thresholding (strong/weak/none)
    uint8_t*  final_out  = nullptr; // Final binary edge map after hysteresis

    // ====================== ALLOCATIONS ======================
    // All buffers are 64-byte aligned. This satisfies alignment requirements
    // for RISC-V Vector (RVV) SIMD instructions, enabling future vectorization
    // of any stage without needing to re-allocate or copy data.
    blurred    = (uint8_t*) aligned_alloc(64, (size_t)n);                   // 1 byte/pixel
    Gx         = (int16_t*)aligned_alloc(64, (size_t)n * sizeof(int16_t));  // 2 bytes/pixel
    Gy         = (int16_t*)aligned_alloc(64, (size_t)n * sizeof(int16_t));  // 2 bytes/pixel
    mag        = (uint8_t*) aligned_alloc(64, (size_t)n);                   // 1 byte/pixel
    dir        = (uint8_t*) aligned_alloc(64, (size_t)n);                   // 1 byte/pixel
    nms_out    = (uint8_t*) aligned_alloc(64, (size_t)n);                   // 1 byte/pixel
    thresh_out = (uint8_t*) aligned_alloc(64, (size_t)n);                   // 1 byte/pixel
    final_out  = (uint8_t*) aligned_alloc(64, (size_t)n);                   // 1 byte/pixel

    // If any allocation failed, abort this pipeline run (goto skips all stages)
    if (!blurred || !Gx || !Gy || !mag || !dir || !nms_out || !thresh_out || !final_out) {
        printf("ERROR: aligned_alloc failed in pipeline: %s\n", label);
        goto cleanup; // Jump directly to free() calls to avoid leaking whatever did allocate
    }

    printf("\n=== Pipeline: %s ===\n", label);
    printf("Timing each stage over %d iterations (using %s):\n", ITERATIONS, timing_name);

    // ====================== PIPELINE STAGES ======================

    // Stage 1: Gaussian Blur
    // Applies a 5×5 Gaussian kernel to reduce noise in the input image.
    // Output `blurred` is a smoothed version of `src`.
    BENCH("Stage 1: Gaussian",      gaussian_blur(src, blurred, width, height));

    // Stage 2a: Sobel Gradient
    // Convolves `blurred` with 3×3 Sobel kernels to compute horizontal (Gx)
    // and vertical (Gy) intensity gradients. Both outputs are int16_t to
    // preserve sign and avoid overflow from the convolution sums.
    BENCH("Stage 2a: Sobel",        sobel_gradient(blurred, Gx, Gy, width, height));

    // Stage 2b: Gradient Magnitude
    // Computes per-pixel edge strength from Gx and Gy.
    // L2 (Euclidean): mag = sqrt(Gx² + Gy²)  — more accurate, slower
    // L1 (Manhattan): mag = |Gx| + |Gy|        — approximate, faster
    // Result is normalized/clamped to uint8 range [0, 255].
    if (use_l2) {
        BENCH("Stage 2b: Magnitude L2", magnitude_l2(Gx, Gy, mag, width, height));
    } else {
        BENCH("Stage 2b: Magnitude L1", magnitude_l1(Gx, Gy, mag, width, height));
    }

    // Stage 2c: Gradient Direction
    // Uses atan2(Gy, Gx) to compute each pixel's gradient angle, then
    // quantizes to one of four directions: 0° (H), 45° (D), 90° (V), 135° (AD).
    // Stored as values 0–3 in `dir`. Used by NMS to compare neighbors
    // along the gradient direction.
    BENCH("Stage 2c: Direction",    direction_compute(Gx, Gy, dir, width, height));

    // Stage 3: Non-Maximum Suppression (NMS)
    // Thins edges to 1-pixel width by zeroing any pixel whose magnitude
    // is not a local maximum along its gradient direction.
    // Compares each pixel in `mag` against its two neighbors in `dir`
    // and keeps only peaks; all others are set to 0.
    BENCH("Stage 3: NMS",           nms_u8(mag, dir, nms_out, width, height));

    // Stage 4: Double Threshold
    // Classifies each NMS pixel into three categories:
    //   strong edge  : mag >= high_thresh (30) → 255
    //   weak edge    : low_thresh (15) <= mag < high_thresh → 128
    //   suppressed   : mag < low_thresh → 0
    // These labels guide the final hysteresis step.
    BENCH("Stage 4: Double Threshold",
          double_threshold(nms_out, thresh_out, width, height, 15, 30));

    // Stage 5: Hysteresis Edge Tracking
    // Finalizes the edge map by retaining weak edges (128) only if they
    // are 8-connected to at least one strong edge (255). Isolated weak
    // edges are discarded. Output `final_out` is a binary edge map (0 or 255).
    BENCH("Stage 5: Hysteresis",    hysteresis(thresh_out, final_out, width, height));

// ---- Optional hex dump of intermediate buffers (debug mode only) ----
#if SHOW_HEX_DUMPS
    char name[64];
    // Helper macro: builds a name string "<prefix>_<suffix>" and dumps the buffer
    #define DUMP(suffix, buf) \
        snprintf(name, sizeof(name), "%s_%s", prefix, suffix); \
        dump_image_hex(name, buf, width, height);
    DUMP("gaussian",  blurred)     // Gaussian-smoothed input
    DUMP("magnitude", mag)         // Gradient magnitude map
    DUMP("nms",       nms_out)     // After non-maximum suppression
    DUMP("threshold", thresh_out)  // After double thresholding
    DUMP("final",     final_out)   // Final Canny edge output
    #undef DUMP
#endif

// ---- Cleanup: free all intermediate buffers ----
// `goto cleanup` from the allocation check jumps here, so these calls
// must be safe even when some pointers are still nullptr (free(nullptr) is a no-op).
cleanup:
    free(blurred); free(Gx); free(Gy); free(mag);
    free(dir); free(nms_out); free(thresh_out); free(final_out);
}

// ===================================================================
// MAIN
// ===================================================================
// Entry point: loads the compile-time embedded image, runs the full
// Canny pipeline twice (L1 and L2 magnitude), and prints a timing
// summary of both runs.
// ===================================================================
int main() {
    // Read image dimensions from the embedded_image.h constants
    const int width  = EMBEDDED_IMAGE_WIDTH;
    const int height = EMBEDDED_IMAGE_HEIGHT;
    const int n      = width * height;  // Total pixel count

    // Sanity check: the embedded byte array length must match width × height.
    // A mismatch means embedded_image.h was generated for a different image
    // and the pointers/indexing throughout the pipeline would be wrong.
    if ((unsigned)n != EMBEDDED_IMAGE_LEN) {
        printf("ERROR: Dimension mismatch! width*height = %d, EMBEDDED_IMAGE_LEN = %u\n",
               n, EMBEDDED_IMAGE_LEN);
        printf("→ Please regenerate embedded_image.h\n");
        return 1;
    }

    printf("Embedded image : %dx%d (%d bytes)\n", width, height, n);
    printf("Timing method  : %s\n", timing_name);

    // Allocate a 64-byte-aligned working copy of the source image.
    // We copy rather than use EMBEDDED_IMAGE directly so pipeline stages
    // can freely read from `src` without risk of aliasing the read-only
    // flash/rodata segment (important for correctness on bare-metal targets).
    uint8_t* src = (uint8_t*)aligned_alloc(64, (size_t)n);
    if (!src) {
        printf("ERROR: Failed to allocate source image buffer\n");
        return 1;
    }
    memcpy(src, EMBEDDED_IMAGE, (size_t)n);

// Optionally dump the raw input image for debugging
#if SHOW_HEX_DUMPS
    dump_image_hex("input", src, width, height);
#endif

    // ====================== L1 PIPELINE ======================
    // Runs the pipeline using the L1 (Manhattan) magnitude approximation.
    // Faster than L2 (no square root), slightly less accurate edge strength.
    // The outer read_time() calls measure total wall time for all
    // ITERATIONS warmup + timed runs inside run_pipeline.
    printf("\n=== Running L1 Pipeline (|Gx| + |Gy|) ===\n");
    uint64_t l1_start = read_time();
    run_pipeline(src, width, height, false, "L1  |Gx| + |Gy|", "l1");
    uint64_t l1_end   = read_time();
    double   l1_ms    = (double)(l1_end - l1_start) / 1e6; // Convert ns → ms

    // ====================== L2 PIPELINE ======================
    // Runs the pipeline using the L2 (Euclidean) magnitude: sqrt(Gx² + Gy²).
    // More accurate gradient magnitude but involves a square root per pixel.
    printf("\n=== Running L2 Pipeline (sqrt(Gx² + Gy²)) ===\n");
    uint64_t l2_start = read_time();
    run_pipeline(src, width, height, true, "L2  sqrt(Gx^2 + Gy^2)", "l2");
    uint64_t l2_end   = read_time();
    double   l2_ms    = (double)(l2_end - l2_start) / 1e6; // Convert ns → ms

    // ====================== FINAL SUMMARY ======================
    // Print total elapsed time for each pipeline variant and the
    // per-pass average (total ÷ ITERATIONS), making it easy to compare
    // L1 vs L2 cost and estimate single-frame latency.
    printf("\n=== Pipeline Runtime Totals (%s) ===\n", timing_name);
    printf("  L1 total time   : %.3f ms\n", l1_ms);
    printf("  L1 single pass  : %.3f ms\n", l1_ms / ITERATIONS);  // Average single-image time
    printf("\n");
    printf("  L2 total time   : %.3f ms\n", l2_ms);
    printf("  L2 single pass  : %.3f ms\n", l2_ms / ITERATIONS);  // Average single-image time

    free(src); // Release the source image buffer
    printf("\n Benchmark completed successfully.\n");
    return 0;
}
