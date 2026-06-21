/*
 * bench_gaussian_padded.cpp
 * =========================
 * Phase 4 "Deeper Idea" Benchmark
 *
 * Compares original gaussian_blur() vs gaussian_blur_padded() at
 * multiple optimization levels to measure the vectorization benefit
 * of removing the boundary check via pre-padding.
 *
 * TIMING METHODOLOGY:
 * -------------------
 * Uses rdtime (RISC-V real-time counter) instead of clock_gettime().
 * Reason: riscv64-unknown-elf-g++ targets bare-metal RISC-V where
 * POSIX syscalls (including clock_gettime) are not available.
 * rdtime reads a fixed-frequency hardware timer directly with no OS
 * dependency. It returns nanoseconds on standard RISC-V implementations.
 * Relative comparisons (original vs padded, O0 vs O3) are valid since
 * both measurements use the same timer and conversion factor.
 * This follows the project guide: "absolute numbers are meaningless,
 * but relative comparisons are valid."
 *
 * SCRATCH BUFFER FIX (KEY CHANGE vs previous version):
 * -----------------------------------------------------
 * The previous version called gaussian_blur_padded() with no scratch
 * argument, so the function allocated and freed ~547 KB on every call.
 * Over 150 iterations this meant 150 malloc+memset+free operations were
 * being timed — measuring allocator overhead, not convolution speed.
 * This is why the padded version appeared 3-4× SLOWER than the original.
 *
 * Fix: allocate scratch ONCE here, memset it to 0 ONCE, then pass the
 * same pointer into every iteration. gaussian_blur_padded() never writes
 * to the border area of scratch, so the zero border stays valid across
 * all 150 iterations without any re-zeroing.
 *
 * ITERATIONS: 200 (increased from 150 for more stable rdtime readings)
 * WARMUP: 1 run per version before timing starts
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <cstring>
#include "gaussian.h"
#include "gaussian_padded.h"

// ── Self-Healing Header Check ──────────────────────────────
#if __has_include("embedded_image.h")
    #include "embedded_image.h"
#elif __has_include("include/embedded_image.h")
    #include "include/embedded_image.h"
#else
    // Fallback definitions if the Makefile hasn't generated the file yet
    #warning "embedded_image.h not found! Using default 136x136 canvas definitions."
    #define EMBEDDED_IMAGE_WIDTH  136
    #define EMBEDDED_IMAGE_HEIGHT 136
    #define EMBEDDED_IMAGE_LEN    (136 * 136)
    // Create a dummy empty array so variables match
    static const uint8_t EMBEDDED_IMAGE[136 * 136] = {0};
#endif

#ifndef OPT_LEVEL
#define OPT_LEVEL "unknown"
#endif

// ... rest of your bench_gaussian_padded.cpp code continues exactly the same ...

#ifndef OPT_LEVEL
#define OPT_LEVEL "unknown"
#endif

// Increased from 150 to 200: rdtime on QEMU has limited resolution;
// more iterations give a more stable per-iteration average.
static const int ITERATIONS = 200;

// ── rdtime: RISC-V real-time counter ─────────────────────────
// Returns nanoseconds since boot (fixed-frequency hardware timer).
// No syscall, no OS dependency — works on bare-metal RISC-V under QEMU.
// "memory" clobber prevents compiler from reordering timer reads.
static inline uint64_t read_time() {
    uint64_t t;
    asm volatile("rdtime %0" : "=r"(t) :: "memory");
    return t;
}



int main() {
    const int width  = EMBEDDED_IMAGE_WIDTH;
    const int height = EMBEDDED_IMAGE_HEIGHT;
    const int n      = width * height;

    // Sanity check: catch stale embedded_image.h
    if ((unsigned)n != EMBEDDED_IMAGE_LEN) {
        printf("ERROR: embedded image size mismatch (%d vs %u)\n",
               n, EMBEDDED_IMAGE_LEN);
        return 1;
    }

    // ── Allocate all buffers once, before any timing ──────────
    // aligned_alloc(64) is required for RVV loads/stores.
    uint8_t* src      = (uint8_t*)aligned_alloc(64, n);
    uint8_t* dst_orig = (uint8_t*)aligned_alloc(64, n);
    uint8_t* dst_pad  = (uint8_t*)aligned_alloc(64, n);

    if (!src || !dst_orig || !dst_pad) {
        printf("ERROR: aligned_alloc failed for image buffers\n");
        return 1;
    }

    // ── Allocate scratch buffer for gaussian_blur_padded() ────
    // Size = (width+4) * (height+4) — one RADIUS=2 ring of zeros
    // around the image on all four sides.
    //
    // WHY HERE and not inside gaussian_blur_padded():
    //   Allocating inside the function means 200 malloc+zero+free
    //   cycles get timed as part of "convolution". That dominated
    //   the previous results, making padded look 3-4× slower.
    //   Allocating here means the timing loop measures only the
    //   actual convolution work — which is what we want to compare.
    const int pw      = width  + 4;
    const int ph      = height + 4;
    const int scratch_size = pw * ph;

    uint8_t* scratch = (uint8_t*)aligned_alloc(64, scratch_size);
    if (!scratch) {
        printf("ERROR: aligned_alloc failed for scratch buffer\n");
        return 1;
    }

    // Zero the scratch buffer ONCE.
    // gaussian_blur_padded() only writes to the centre rows (via memcpy),
    // never to the border columns/rows. So the border stays zero across
    // ALL iterations — no re-zeroing needed inside the timing loop.
    memset(scratch, 0, scratch_size);

    // Copy embedded image into src
    memcpy(src, EMBEDDED_IMAGE, n);

    // volatile sink prevents the compiler from eliminating timed loops
    // (a loop whose output is never read can be optimised away entirely)
    volatile uint8_t sink = 0;

    // ── Warmup (not timed) ───────────────────────────────────
    // One warm-up call per version stabilises the instruction cache
    // and branch predictor so the first timed iteration isn't cold.
    gaussian_blur(src, dst_orig, width, height);
    gaussian_blur_padded(src, dst_pad, scratch, width, height);

    uint64_t probe0 = read_time();
volatile int x = 0;
for (int i = 0; i < 1000; i++) x += i;
uint64_t probe1 = read_time();
printf("PROBE: delta = %llu ns (should be nonzero, plausible for 1000 adds)\n",
       (unsigned long long)(probe1 - probe0));

    // ── Benchmark: Original gaussian_blur() ──────────────────
    // Times only the convolution + boundary-check logic.
    // No allocation inside this function.
    uint64_t t0 = read_time();
    for (int i = 0; i < ITERATIONS; i++) {
        gaussian_blur(src, dst_orig, width, height);
        sink ^= dst_orig[0];   // read result to prevent dead-code elimination
    }
    uint64_t t_orig_ticks = read_time() - t0;

    // Per-iteration time in milliseconds.
    // rdtime is in nanoseconds on QEMU → divide by 1e6 for ms.
    double t_orig_ms = (double)t_orig_ticks / 1e6 / ITERATIONS;

    // ── Benchmark: gaussian_blur_padded() ────────────────────
    // scratch is already allocated and zeroed above — this loop
    // measures ONLY the memcpy + branch-free convolution.
    t0 = read_time();
    for (int i = 0; i < ITERATIONS; i++) {
        gaussian_blur_padded(src, dst_pad, scratch, width, height);
        sink ^= dst_pad[0];    // prevent dead-code elimination
    }
    uint64_t t_pad_ticks = read_time() - t0;

    double t_pad_ms = (double)t_pad_ticks / 1e6 / ITERATIONS;

    // ── Correctness Check ─────────────────────────────────────
    // Both versions use the same GAUSSIAN_KERNEL and GAUSSIAN_DIVISOR,
    // and the same zero-padding strategy, so output should be identical.
    // Any difference would be a bug, not a rounding artifact.
    int max_diff   = 0;
    int diff_count = 0;
    for (int i = 0; i < n; i++) {
        int diff = (int)dst_orig[i] - (int)dst_pad[i];
        if (diff < 0) diff = -diff;
        if (diff > max_diff) max_diff = diff;
        if (diff > 0) diff_count++;
    }

    // ── Speedup ───────────────────────────────────────────────
    // Positive = padded is faster than original.
    // Negative = padded is slower (should not happen after the fix).
    double speedup = (t_orig_ms > 0.0)
                   ? (t_orig_ms - t_pad_ms) / t_orig_ms * 100.0
                   : 0.0;

    // ── Print Results ─────────────────────────────────────────
    printf("\n");
    printf("+----------------------------------------------------------+\n");
    printf("|  Gaussian Blur Comparison  OPT: %-15s         |\n", OPT_LEVEL);
    printf("|  Image: %dx%d  Iterations: %d                    |\n",
           width, height, ITERATIONS);
    printf("|  Timing: clock_gettime(CLOCK_MONOTONIC)  Warmup: 1 run |\n");
    printf("|  Scratch: pre-allocated %d bytes (zeroed once)    |\n",
           scratch_size);
    printf("+------------------------+-----------+--------------------+\n");
    printf("|  Version               |  Time(ms) | vs Original        |\n");
    printf("+------------------------+-----------+--------------------+\n");
    printf("|  Original (boundary)   | %9.4f |  baseline          |\n",
           t_orig_ms);
    printf("|  Padded  (no branch)   | %9.4f | %+8.2f%%            |\n",
           t_pad_ms, speedup);
    printf("+------------------------+-----------+--------------------+\n");
    printf("|  Correctness: max_diff=%-3d  diff_pixels=%-6d          |\n",
           max_diff, diff_count);
    printf("+----------------------------------------------------------+\n");

    // Machine-readable line for the sweep script to parse.
    // Format: GAUSSIAN_RESULT|LEVEL|t_orig_ms|t_pad_ms|speedup%|max_diff|diff_count
    printf("GAUSSIAN_RESULT|%s|%.6f|%.6f|%.2f|%d|%d\n",
           OPT_LEVEL, t_orig_ms, t_pad_ms, speedup, max_diff, diff_count);


           // Dump both outputs for visual correctness verification
printf("=== DUMP_START name=gaussian_original width=%d height=%d ===\n", width, height);
for (int i = 0; i < n; i++) {
    printf("%02x", dst_orig[i]);
    if ((i + 1) % 32 == 0) printf("\n");
}
if (n % 32 != 0) printf("\n");
printf("=== DUMP_END ===\n");

printf("=== DUMP_START name=gaussian_padded width=%d height=%d ===\n", width, height);
for (int i = 0; i < n; i++) {
    printf("%02x", dst_pad[i]);
    if ((i + 1) % 32 == 0) printf("\n");
}
if (n % 32 != 0) printf("\n");
printf("=== DUMP_END ===\n");

    free(scratch);
    free(src);
    free(dst_orig);
    free(dst_pad);
    return 0;
}



