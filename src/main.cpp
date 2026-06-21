#include <cstdio>
#include <cstdlib>
#include "image_io.h"
#include "gaussian.h"
#include "sobel.h"
#include "magnitude.h"
#include "direction.h"
#include "nms.h"
#include "double_threshold.h"
#include "hysteresis.h"

/*
 * Canny Edge Detection Pipeline
 * ==============================
 * Full 5-stage implementation:
 *
 * Stage 1: Gaussian Blur       → reduce noise before edge detection
 * Stage 2: Sobel Gradients     → find intensity changes (Gx, Gy)
 *          Magnitude           → combine Gx and Gy into edge strength
 *          Direction           → find edge orientation (0,45,90,135 degrees)
 * Stage 3: Non-Maximum         → thin edges to 1-pixel width
 *          Suppression (NMS)
 * Stage 4: Double Threshold    → classify edges as strong/weak/none
 * Stage 5: Hysteresis          → keep weak edges connected to strong edges
 *
 * Usage:
 *   ./canny_rv <input.raw> <width> <height>
 *
 * Input:  raw grayscale image (width * height bytes, no header)
 * Output: multiple .raw files, one per stage
 */

int main(int argc, char* argv[]) {

    // ─────────────────────── Argument validation ────────────────────────
    // We need exactly 3 arguments: input file, width, height
    // argc counts the program name too, so we check for 4
    if (argc != 4) {
        printf("Usage: %s <input.raw> <width> <height>\n", argv[0]);
        return 1;
    }

    const char* input_path = argv[1];
    int width  = atoi(argv[2]);   // convert string to int
    int height = atoi(argv[3]);
    int n = width * height;       // total number of pixels

    // ──────────────────── Stage 0: Load input image ──────────────────────
    // aligned_alloc(64, n) allocates n bytes aligned to 64-byte boundary
    // 64-byte alignment is required for RVV vector loads (vle8/vle32)
    // and helps the compiler auto-vectorize loads and stores
    uint8_t* src = load_image(input_path, width, height);
    if (src == nullptr) {
        printf("Error: could not load %s\n", input_path);
        free(src);
        return 1;
    }
    printf("Loaded: %s (%dx%d)\n", input_path, width, height);

    // ────────────────────── Stage 1: Gaussian Blur ───────────────────────
    // Purpose: smooth the image to reduce noise
    // Without this, random pixel variations would be detected as edges
    // Uses a 5x5 kernel with sigma~1.0 and zero-padding at borders
    uint8_t* blurred = (uint8_t*)aligned_alloc(64, n);
    gaussian_blur(src, blurred, width, height);
    save_image("out_gaussian.raw", blurred, width, height);
    printf("Stage 1 done: Gaussian blur\n");

    // ───────────────────── Stage 2a: Sobel Gradients ─────────────────────
    // Purpose: detect intensity changes in X and Y directions
    //
    // Gx detects vertical edges   (left-right intensity changes)
    // Gy detects horizontal edges (top-bottom intensity changes)
    //
    // We use int16_t because Sobel output ranges from -1020 to +1020
    // (max pixel value 255 × max kernel coefficient 4 × 2 = 2040 → fits in int16_t)
    //
    // So A layout (separate Gx, Gy arrays) is chosen over AoS (interleaved)
    // because it allows efficient vector loads in the RVV optimization stage
    int16_t* Gx = (int16_t*)aligned_alloc(64, n * sizeof(int16_t));
    int16_t* Gy = (int16_t*)aligned_alloc(64, n * sizeof(int16_t));
    sobel_gradient(blurred, Gx, Gy, width, height);
    printf("Stage 2a done: Sobel gradients\n");

    // ────────────────────── Stage 2b: Gradient Magnitude ─────────────────
    // Purpose: combine Gx and Gy into a single edge strength value
    //
    // We use L2 norm: sqrt(Gx² + Gy²) — mathematically correct
    // L1 norm: |Gx| + |Gy| is faster but overestimates diagonal edges
    // Output is normalized to [0,255] so it can be saved as uint8_t image
    uint8_t* mag = (uint8_t*)aligned_alloc(64, n);
    magnitude_l2(Gx, Gy, mag, width, height);
    save_image("out_magnitude.raw", mag, width, height);
    printf("Stage 2b done: Gradient magnitude\n");

    // ───────────────────── Stage 2c: Gradient Direction ─────────────────
    // Purpose: find the orientation of each edge
    //
    // Instead of computing exact angle with atan2() (expensive),
    // we quantize to 4 directions using integer comparisons:
    //   0  → 0°   horizontal gradient → vertical edge
    //   1  → 45°  diagonal
    //   2  → 90°  vertical gradient   → horizontal edge
    //   3  → 135° diagonal (opposite)
    //
    // This quantized direction is used by NMS to compare the right neighbors
    uint8_t* dir = (uint8_t*)aligned_alloc(64, n);
    direction_compute(Gx, Gy, dir, width, height);
    save_image("out_direction.raw", dir, width, height);
    printf("Stage 2c done: Gradient direction\n");

    // ───────────── Stage 3: Non-Maximum Suppression (NMS) ───────────────
    // Purpose: thin edges from several pixels wide to exactly 1 pixel wide
    //
    // For each pixel, compare its magnitude to its two neighbors
    // along the gradient direction:
    //   - If it is the LOCAL MAXIMUM → keep it
    //   - If it is NOT the maximum   → suppress it (set to 0)
    // Border pixels are always set to 0 (zero-padding approach)
    uint8_t* nms_out = (uint8_t*)aligned_alloc(64, n);
    nms_u8(mag, dir, nms_out, width, height);
    save_image("out_nms.raw", nms_out, width, height);
    printf("Stage 3 done: NMS\n");

    // ────────────────────── Stage 4: Double Threshold ────────────────────
    // Purpose: classify edges into strong, weak, or none
    //
    // Thresholds are computed automatically from the magnitude histogram
    // instead of hardcoded values, so they adapt to any image/resolution:
    //
    //   high_thresh = max_magnitude * high_ratio  (0.2)
    //   low_thresh  = high_thresh   * low_ratio   (0.5)
    //
    // This ensures consistent edge detection regardless of image contrast
    uint8_t* thresh_out = (uint8_t*)aligned_alloc(64, n);

    // Find max magnitude after NMS
    uint8_t max_mag = 0;
    for (int i = 0; i < n; i++)
        if (nms_out[i] > max_mag) max_mag = nms_out[i];

    uint8_t high_thresh = (uint8_t)(max_mag * 0.2f);
    uint8_t low_thresh  = (uint8_t)(high_thresh * 0.5f);

    printf("Auto threshold: high=0.2*max_mag  low=0.5*high\n");

    double_threshold(nms_out, thresh_out, width, height, low_thresh, high_thresh);
    save_image("out_threshold.raw", thresh_out, width, height);
    printf("Stage 4 done: Double threshold\n");

    // ───────────────── Stage 5: Hysteresis Edge Tracing ──────────────────
    // Purpose: decide which weak edges to keep
    //
    // Rule: a weak edge (128) is kept ONLY if it is connected to a strong edge (255)
    // Algorithm:
    //   1. Start from every strong pixel
    //   2. Check all 8 neighbors
    //   3. If a neighbor is weak → promote it to strong (255)
    //   4. Repeat until no more changes (iterative propagation)
    //   5. Suppress all remaining weak pixels (set to 0)
    //
    // Result: final binary edge map (255 = edge, 0 = background)
    uint8_t* final_out = (uint8_t*)aligned_alloc(64, n);
    hysteresis(thresh_out, final_out, width, height);
    save_image("out_final.raw", final_out, width, height);
    printf("Stage 5 done: Hysteresis\n");

    printf("\nCanny pipeline complete! Output files:\n");
    printf("  out_gaussian.raw   → after Gaussian blur\n");
    printf("  out_magnitude.raw  → gradient magnitude\n");
    printf("  out_nms.raw        → after NMS\n");
    printf("  out_threshold.raw  → after double threshold\n");
    printf("  out_final.raw      → final edge map\n");

    // ────────────────────────────── Cleanup ────────────────────────────
    // Free all allocated buffers
    // Each aligned_alloc must have exactly one matching free()
    free(src);
    free(blurred);
    free(Gx);
    free(Gy);
    free(mag);
    free(dir);
    free(nms_out);
    free(thresh_out);
    free(final_out);

    return 0;
}
