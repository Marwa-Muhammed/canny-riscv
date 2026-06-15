#include "gaussian.h"

// Generic convolution with zero-padding boundary handling.
// Zero-padding: pixels outside image boundaries are treated as 0.
// PixelType  → type of image pixels  (uint8_t)
// AccumType  → type of accumulator   (int32_t) to avoid overflow
// KernelType → type of kernel values (int16_t)
template<typename PixelType, typename AccumType, typename KernelType>
void convolve(const PixelType* src, PixelType* dst,
    int width, int height,
    const KernelType* kernel,
    int ksize, AccumType divisor) {

    int half = ksize / 2;  // = 2 for 5x5 kernel

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            AccumType sum = 0;  // accumulator (32-bit to avoid overflow)

            /*
           When the kernel is centered on pixel (y,x), we want the kernel center (half,half) as it is 5*5
           to align with (x,y). So for any kernel position (kr, kc), the image pixel we need is (y + ky - half,x + kx - half)
           */
            for (int ky = 0; ky < ksize; ky++) {
                for (int kx = 0; kx < ksize; kx++) {

                    int ir = y + ky - half;  // image row to read
                    int ic = x + kx - half;  // image col to read

                    // Zero-padding: out-of-bounds pixels contribute 0
                    PixelType pixel = 0;
                    if (ir >= 0 && ir < height && ic >= 0 && ic < width) {
                        pixel = src[ir * width + ic];
                    }

                    sum += static_cast<AccumType>(pixel) *
                        static_cast<AccumType>(kernel[ky * ksize + kx]);
                }
            }

            // Normalize by divisor, clamp to [0, 255]
            AccumType result = sum / divisor;
            if (result < 0)   result = 0;
            if (result > 255) result = 255;

            dst[y * width + x] = static_cast<PixelType>(result);
        }
    }
}

// Gaussian blur: calls convolve with the 5x5 Gaussian kernel
void gaussian_blur(const uint8_t* src, uint8_t* dst,
    int width, int height) {
    convolve<uint8_t, int32_t, int16_t>(
        src, dst, width, height,
        &GAUSSIAN_KERNEL[0][0],
        5,
        GAUSSIAN_DIVISOR
    );
}
