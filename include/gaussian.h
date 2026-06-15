//Include this file only once, even if multiple files include it
#pragma once
//Brings in fixed-size integer types
#include <cstdint>
#include <cstddef>

// 5x5 Gaussian kernel coefficients (sigma ~ 1.0)
// Sum = 273, using integer arithmetic
static const int16_t GAUSSIAN_KERNEL[5][5] = {
{ 1,  4,  7,  4,  1},
{ 4, 16, 26, 16,  4},
{ 7, 26, 41, 26,  7},
{ 4, 16, 26, 16,  4},
{ 1,  4,  7,  4,  1}
};

static const int32_t GAUSSIAN_DIVISOR = 273;

// Generic convolution template
// PixelType  → type of image pixels (e.g. uint8_t)
// AccumType  → type of accumulator  (e.g. int32_t)
// KernelType → type of kernel coefficients (e.g. int16_t)
template<typename PixelType, typename AccumType, typename KernelType>
void convolve(const PixelType* src, PixelType* dst,
              int width, int height,
              const KernelType* kernel,
              int ksize, AccumType divisor);

// Gaussian blur using 5x5 kernel with zero-padding
void gaussian_blur(const uint8_t* src, uint8_t* dst,
                   int width, int height);
