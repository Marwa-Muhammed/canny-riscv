#include <stdio.h>   // printf
#include <time.h>    // clock_gettime
#include <stdint.h>  // uint8_t
#include <stdlib.h>  // aligned_alloc, free
#include "gaussian.h" // gaussian_blur function

// Returns elapsed time in milliseconds
double get_time_ms() {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec * 1000.0 + ts.tv_nsec / 1e6;
}

int main() {
    const int W = 512, H = 512;
    uint8_t* src = (uint8_t*)aligned_alloc(64, W * H);
    uint8_t* dst = (uint8_t*)aligned_alloc(64, W * H);

    // Fill with test pattern
    for (int i = 0; i < W * H; i++)
        src[i] = i % 256;

    const int ITERATIONS = 100;

    // Measure Gaussian blur
    double start = get_time_ms();
    for (int i = 0; i < ITERATIONS; i++)
        gaussian_blur(src, dst, W, H);
    double end = get_time_ms();

    printf("Gaussian blur: %.3f ms (avg over %d runs)\n",
           (end - start) / ITERATIONS, ITERATIONS);

    free(src);
    free(dst);
    return 0;
}
