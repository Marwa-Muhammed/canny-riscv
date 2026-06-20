#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

#include "gaussian_vectorized.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude_vectorized.h"
#include "magnitude.h"
#include "direction.h"
#include "nms.h"
#include "double_threshold.h"
#include "hysteresis.h"
#include "embedded_image.h"

/*
 * riscv_main_vectorized.cpp
 * ========================
 * Entry point for the RVV-accelerated pipeline.
 *
 * Runs the full Canny edge detector on the embedded image TWICE:
 *
 *   Run 1 ("RVV")    - Gaussian blur and magnitude use the RVV
 *                       (vectorized) implementations.
 *   Run 2 ("Scalar") - Gaussian blur and magnitude use the original
 *                       scalar implementations instead.
 *
 * Every other stage (Sobel gradients, direction, non-max suppression,
 * double threshold, hysteresis) is scalar-only in BOTH runs - there is
 * no vectorized alternative for them, so they are simply re-run
 * identically each time to keep the two pipelines structurally apples-
 * to-apples (same stage order, same buffer allocation pattern, same
 * timing bracket placement around every stage). Both runs are driven by
 * a single run_pipeline() function that takes a flag selecting which
 * Gaussian/magnitude implementation to use - this guarantees the two
 * runs execute the exact same code path rather than two hand-written
 * copies that could silently drift apart.
 *
 * The image is embedded into the binary because fopen() is unavailable
 * under the bare-metal RISC-V toolchain running in QEMU.
*/
// ------------------- RISC-V Timing -------------------
// rdtime was replaced here: QEMU's virtual clock backing rdtime ticks at
// a fixed virtual rate (commonly 10 MHz under qemu-riscv64) that does not
// track actual instruction execution time, so it doesn't measure real
// elapsed time -- only a number that happens to increase.
//
// clock_gettime() (real implementation in qemu_clock.cpp, linked in
// separately) issues the actual Linux clock_gettime syscall via `ecall`.
// qemu-riscv64 is QEMU's user-mode emulator, so it intercepts that ecall
// and forwards it to the REAL host kernel -- this returns genuine host
// wall-clock time, the same CLOCK_MONOTONIC source the host build uses,
// which is what actually makes these numbers meaningful in milliseconds.
#include <time.h>
#ifndef CLOCK_MONOTONIC
#define CLOCK_MONOTONIC 1
#endif
extern "C" int clock_gettime(int clk_id, struct timespec* tp);

static inline uint64_t read_time() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}
const char* timing_name = "clock_gettime (real wall-clock, ns)";

static constexpr int TIMING_REPEATS = 200; // tune so each stage's total is >> timer resolution
// ---------------------------------------------------------------------
// Stage timing table.
//
// Each pipeline stage gets one slot holding its name plus the elapsed
// cycle count from BOTH runs (RVV and Scalar), so we can compute a
// percentage speedup/slowdown per stage. NUM_STAGES must match the
// number of record_stage() calls made per run_pipeline() call below.
// ---------------------------------------------------------------------
static constexpr int NUM_STAGES = 7;

struct StageTiming {
    const char* name;
    uint64_t    rvv_cycles;
    uint64_t    scalar_cycles;
};

static StageTiming g_stage_timings[NUM_STAGES];


static void record_stage(int stage_idx, const char* name, uint64_t start, uint64_t end, bool is_rvv_run) {
    // Keep raw delta resolution intact so we don't truncate to 0!
    uint64_t elapsed = end - start; 

    if (is_rvv_run) {
        g_stage_timings[stage_idx].name         = name;
        g_stage_timings[stage_idx].rvv_cycles    = elapsed;
    } else {
        g_stage_timings[stage_idx].scalar_cycles = elapsed;
    }
}
// Prints the RVV vs Scalar comparison table: each stage's cycle count
// under both implementations, plus the percentage speedup/slowdown of
// RVV relative to Scalar, and a TOTAL row summed across all stages.

static void print_comparison_table() {
    // Clarify unit metrics inside the table header header
    printf("\n=== RVV vs SCALAR STAGE TIMING (clock_gettime, values in Milliseconds) ===\n");
    printf("%-22s | %14s | %14s\n",
           "Stage", "RVV (ms)", "Scalar (ms)");
    printf("-----------------------+----------------+----------------+-----------\n");

    uint64_t total_rvv = 0, total_scalar = 0;

    for (int i = 0; i < NUM_STAGES; i++) {
    uint64_t rvv    = g_stage_timings[i].rvv_cycles;
    uint64_t scalar = g_stage_timings[i].scalar_cycles;

    total_rvv    += rvv;
    total_scalar += scalar;

    

    printf("%-22s | %14.3f | %14.3f \n",
           g_stage_timings[i].name,
           (double)rvv    / TIMING_REPEATS / 1e6,
           (double)scalar / TIMING_REPEATS / 1e6);
}

printf("-----------------------+----------------+----------------+-----------\n");



printf("%-22s | %14.3f | %14.3f \n",
       "TOTAL",
       (double)total_rvv    / TIMING_REPEATS / 1e6,
       (double)total_scalar / TIMING_REPEATS / 1e6 );
    printf("===========================================================================\n");

}
// ---------------------------------------------------------------------
// Dump a buffer as hexadecimal text.
// decoder_dump.py reconstructs .raw files from these dumps, so the
// marker format must remain unchanged.
// ---------------------------------------------------------------------
static void dump_image_hex(const char* stage_name,
                           const uint8_t* buffer,
                           int width,
                           int height)
{
    printf("=== DUMP_START name=%s width=%d height=%d ===\n",
           stage_name, width, height);

    int total = width * height;

    for (int i = 0; i < total; i++) {
        printf("%02x", buffer[i]);

        if ((i + 1) % 32 == 0)
            printf("\n");
    }

    if (total % 32 != 0)
        printf("\n");

    printf("=== DUMP_END ===\n");
}


// ---------------------------------------------------------------------
// run_pipeline
//
// Runs the full 7-stage Canny pipeline once, on a fresh copy of the
// embedded image, and records each stage's elapsed cycle count. Both
// the "RVV" and "Scalar" runs call this SAME function with a different
// `use_rvv` flag - this is what guarantees the two runs are structurally
// identical (same stage order, same buffer allocation pattern, same
// timing bracket placement) rather than two hand-maintained copies that
// could quietly drift apart over time.
//
// use_rvv : true  -> Gaussian blur and magnitude use the RVV functions
//           false -> Gaussian blur and magnitude use the scalar functions
// do_dump : true  -> also produce the DUMP_START/DUMP_END hex dumps
//                     (only meant to be true for the RVV run, to keep
//                     decoder_dump.py's expected output unchanged)
// ---------------------------------------------------------------------
static void run_pipeline(const uint8_t* src, int width, int height,
                          bool use_rvv, bool do_dump)
{
    const int n = width * height;

    // -------------------------------------------------------------
    // Fresh buffers for this run. Allocating a brand-new buffer set
    // per run (rather than reusing one set across both runs) avoids
    // any chance that stage N's timing in run 2 is measuring work on
    // top of stale data left over from run 1.
    // -------------------------------------------------------------
    uint8_t* blurred    = (uint8_t*) aligned_alloc(64, n);

    int16_t* gx         = (int16_t*) aligned_alloc(64, n * sizeof(int16_t));
    int16_t* gy         = (int16_t*) aligned_alloc(64, n * sizeof(int16_t));

    uint8_t* magnitude  = (uint8_t*) aligned_alloc(64, n);
    uint8_t* direction  = (uint8_t*) aligned_alloc(64, n);
    uint8_t* nms_out    = (uint8_t*) aligned_alloc(64, n);
    uint8_t* thresh_out = (uint8_t*) aligned_alloc(64, n);
    uint8_t* final_out  = (uint8_t*) aligned_alloc(64, n);

    // NOTE: t_start and t_end are both RAW TIMESTAMPS from read_time()
    // here - neither is pre-subtracted. record_stage() is the only
    // place the (end - start) subtraction happens. See the comment on
    // record_stage() above for why this matters.
    uint64_t t_start, t_end;

    // ===============================================================
    // Stage 1 : Gaussian blur (RVV or scalar, depending on use_rvv)
    //
    // Smooth the image before edge detection to suppress noise.
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    if (use_rvv) {
        gaussian_blur_rvv(src, blurred, width, height);
    } else {
        gaussian_blur(src, blurred, width, height);
    }
}
    t_end = read_time();
    record_stage(0, "Gaussian blur", t_start, t_end, use_rvv);


    // ===============================================================
    // Stage 2a : Sobel gradients (scalar - no RVV alternative exists,
    // identical call in both runs)
    //
    // Compute horizontal and vertical intensity changes.
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    sobel_gradient(blurred, gx, gy, width, height);
    }
    t_end = read_time();
    record_stage(1 , "Sobel gradient", t_start, t_end, use_rvv);


    // ===============================================================
    // Stage 2b : Gradient magnitude (RVV or scalar, depending on use_rvv)
    //
    // Compute L1 magnitude:
    //
    //      |Gx| + |Gy|
    //
    // and normalize it into [0,255].
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    if (use_rvv) {
        sobel_magnitude_rvv(gx, gy, magnitude, width, height);
    } else {
        magnitude_l1(gx, gy, magnitude, width, height);
    }
}
    t_end = read_time();
    record_stage(2, "Magnitude", t_start, t_end, use_rvv);


    // ===============================================================
    // Stage 2c : Gradient direction (scalar - no RVV alternative
    // exists, identical call in both runs)
    //
    // Needed later by non-maximum suppression.
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    direction_compute(gx, gy, direction, width, height);
    }
    t_end = read_time();
    record_stage(3 , "Direction", t_start, t_end, use_rvv);


    // ===============================================================
    // Stage 3 : Non-Maximum Suppression (scalar - identical call in
    // both runs)
    //
    // Thin thick gradient ridges down to one-pixel-wide edges.
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    nms_u8(magnitude, direction, nms_out, width, height);
    }
    t_end = read_time();
    record_stage(4 , "Non-max suppression", t_start, t_end, use_rvv);


    // ===============================================================
    // Stage 4 : Double threshold (scalar - identical call in both runs)
    //
    // Classify pixels into strong, weak, and non-edge.
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    double_threshold(nms_out, thresh_out, width, height, 15, 30);
    }
    t_end = read_time();
    record_stage(5 , "Double threshold", t_start, t_end, use_rvv);


    // ===============================================================
    // Stage 5 : Hysteresis (scalar - identical call in both runs)
    //
    // Keep weak edges connected to strong edges and discard the rest.
    // ===============================================================
    t_start = read_time();
    for (int r = 0; r < TIMING_REPEATS; r++) {
    hysteresis(thresh_out, final_out, width, height);
    }
    t_end = read_time();
    record_stage(6,"Hysteresis", t_start, t_end, use_rvv);


    // ---------------------------------------------------------------
    // Dump intermediate and final images for decoding on the host.
    // Only happens when do_dump is true (the RVV run), so the
    // DUMP_START/DUMP_END marker sequence that decoder_dump.py
    // depends on stays exactly as it was before this comparison
    // feature was added.
    // ---------------------------------------------------------------
    if (do_dump) {
        dump_image_hex("input",      src,        width, height);
        dump_image_hex("gaussian",   blurred,    width, height);
        dump_image_hex("magnitude",  magnitude,  width, height);
        dump_image_hex("nms",        nms_out,    width, height);
        dump_image_hex("threshold",  thresh_out, width, height);
        dump_image_hex("final",      final_out,  width, height);
    }

    // ---------------------------------------------------------------
    // Cleanup (this run's buffers only - src is owned by the caller)
    // ---------------------------------------------------------------
    free(blurred);
    free(gx);
    free(gy);
    free(magnitude);
    free(direction);
    free(nms_out);
    free(thresh_out);
    free(final_out);
}


int main()
{
    const int width  = EMBEDDED_IMAGE_WIDTH;
    const int height = EMBEDDED_IMAGE_HEIGHT;
    const int n      = width * height;

    // ---------------------------------------------------------------
    // Verify that the dimensions agree with the embedded array size.
    // This catches stale embedded_image.h files immediately.
    // ---------------------------------------------------------------
    if ((unsigned)n != EMBEDDED_IMAGE_LEN) {
        printf("ERROR: width*height (%d) != EMBEDDED_IMAGE_LEN (%u)\n",
               n, EMBEDDED_IMAGE_LEN);
        return 1;
    }

    printf("Embedded image: %dx%d (%d bytes)\n", width, height, n);

    // ---------------------------------------------------------------
    // Copy the embedded image into a heap buffer. Both runs read from
    // this same source image (read-only), so they are processing
    // identical input.
    // ---------------------------------------------------------------
    uint8_t* src = (uint8_t*)aligned_alloc(64, n);
    memcpy(src, EMBEDDED_IMAGE, n);

    // ===============================================================
    // Run 1: RVV pipeline (Gaussian + Magnitude vectorized).
    // This run also produces the image dumps, exactly as before.
    // ===============================================================

    run_pipeline(src, width, height, /*use_rvv=*/true, /*do_dump=*/true);

    // ===============================================================
    // Run 2: Scalar pipeline (Gaussian + Magnitude scalar).
    // No image dumps - this run exists purely for timing comparison.
    // ===============================================================

    run_pipeline(src, width, height, /*use_rvv=*/false, /*do_dump=*/false);

    // ---------------------------------------------------------------
    // Print the RVV vs Scalar comparison table + percentage
    // speedup/slowdown per stage, after both runs and all dumps.
    // ---------------------------------------------------------------
    print_comparison_table();

    free(src);

    return 0;
}
