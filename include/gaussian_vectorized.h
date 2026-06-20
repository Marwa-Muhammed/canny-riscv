#include "gaussian.h"
#include <riscv_vector.h>
#include <cstdint>

/**
 *  Apply a 5×5 Gaussian blur to the interior region of an image using
 *        the RISC-V Vector Extension (RVV).
 *
 * This function performs vectorized convolution only on pixels whose entire
 * 5×5 neighborhood lies inside the image. Border pixels are intentionally
 * skipped and should be handled separately (e.g. with scalar code and
 * zero-padding).
 *
 * Input and output images are stored as contiguous arrays of uint8_t pixels
 * in row-major order.
 *
 * Image layout:
 *      pixel(y,x) = image[y * width + x]
 *
 *  src
 *      Pointer to the source image.
 *
 * dst
 *      Pointer to the destination image. Must have the same dimensions as
 *      the source image.
 *
 * width
 *      Image width in pixels.
 *
 *  height
 *      Image height in pixels.
 */
void gaussian_blur_interior_rvv(
    const uint8_t* src,
    uint8_t* dst,
    int width,
    int height);

/**
 *  Apply a 5×5 Gaussian blur to the border region of an image using
 *        scalar (non-vectorized) code with zero-padding.
 *
 * This function computes only the pixels excluded by
 * gaussian_blur_interior_rvv(): the top/bottom border rows and the
 * left/right border columns. Any 5×5 kernel tap that falls outside the
 * image bounds contributes zero to the sum (zero-padding), matching the
 * boundary behavior of the reference scalar gaussian_blur().
 *
 * Input and output images are stored as contiguous arrays of uint8_t pixels
 * in row-major order.
 *
 * Image layout:
 *      pixel(y,x) = image[y * width + x]
 *
 *  src
 *      Pointer to the source image.
 *
 * dst
 *      Pointer to the destination image. Must have the same dimensions as
 *      the source image.
 *
 * width
 *      Image width in pixels.
 *
 *  height
 *      Image height in pixels.
 */
void gaussian_blur_border_scalar(
    const uint8_t* src,
    uint8_t* dst,
    int width,
    int height);

/**
 *  Apply a full 5×5 Gaussian blur to an entire image using a hybrid
 *        vectorized/scalar approach.
 *
 * Combines gaussian_blur_interior_rvv() (fast RVV path for interior pixels)
 * and gaussian_blur_border_scalar() (scalar zero-padded path for border
 * pixels) so every pixel is computed exactly once, with no redundant work.
 * This is the recommended top-level entry point for production use and
 * performance measurement.
 *
 * Input and output images are stored as contiguous arrays of uint8_t pixels
 * in row-major order.
 *
 * Image layout:
 *      pixel(y,x) = image[y * width + x]
 *
 *  src
 *      Pointer to the source image.
 *
 * dst
 *      Pointer to the destination image. Must have the same dimensions as
 *      the source image.
 *
 * width
 *      Image width in pixels.
 *
 *  height
 *      Image height in pixels.
 */
void gaussian_blur_rvv(
    const uint8_t* src,
    uint8_t* dst,
    int width,
    int height);