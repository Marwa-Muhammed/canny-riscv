#include "sobel.h"


template<typename PixelType,typename AccumulatorType,typename KernelType>
void sobel_convolve(const PixelType* src,                    // image resulted from gaussian function
                    int16_t* dist,                           // Pointer to distantion array to store the gradient values for each pixel
                    int width,                              // Width that represents the number of image columns
                    int height,                            // hight that represents the number of image rows
                    const KernelType* kernel,
                    int kernelsize){

    // Loop over every pixel in the image

     int middle = kernelsize / 2;   // in case of (3x3) sobel kernel, half of 3 = 1

    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++) {

            // Accumulators for Gx and Gy
            // int32_t to avoid overflow during multiplication
            AccumulatorType sum = 0;
           
            // Loop over 3x3 kernel
            for (int ky = 0; ky < kernelsize; ky++) {
                for (int kx = 0; kx < kernelsize; kx++) {

                    // Find the image pixel this kernel position maps to
                    int ir = y + ky - middle;  
                    int ic = x + kx - middle;

                    // Zero-padding: if any pixel placed outside image use 0
                    PixelType pixel = 0;
                    
                    if (ir >= 0 && ir < height &&
                        ic >= 0 && ic < width) {
                        pixel = src[ir * width + ic];
                    }

                    // Multiply (matrix multiplication) pixel by kernel weight and accumulate
                    sum += static_cast<AccumulatorType>(pixel) *
                             static_cast<AccumulatorType>(kernel[ky*kernelsize+kx]);  

                }
            }

            // Store results directly, no normalization needed for Sobel
            // Range is -1020 to +1020
             dist[y * width + x] = static_cast<int16_t>(sum);

        }
    }
} 




// Wrapper function — calls template twice (for Gx and Gy)
void sobel_gradient(const uint8_t* src,
                    int16_t* Gx,
                    int16_t* Gy,
                    int width,
                    int height) {

     // Compute Gx using SOBEL_X kernel
    sobel_convolve<uint8_t, int32_t, int16_t>(
        src, Gx, width, height,
        &SOBEL_X[0][0],
        SOBEL_SIZE
    );

    // Compute Gy using SOBEL_Y kernel
    sobel_convolve<uint8_t, int32_t, int16_t>(
        src, Gy, width, height,
        &SOBEL_Y[0][0],
        SOBEL_SIZE
    );
}
// Explicit template instantiation
template void sobel_convolve<uint8_t, int32_t, int16_t>(
    const uint8_t*, int16_t*, int, int, const int16_t*, int);