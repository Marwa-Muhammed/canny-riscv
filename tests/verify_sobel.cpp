#include <iostream>
#include <cstdlib>
#include <cstdint>
#include "../include/sobel.h"

void print_grid(const char* name, const int16_t* arr, int width, int height) {
    std::cout << "\n" << name << ":\n";
    for (int y = 0; y < height; y++) {
        for (int x = 0; x < width; x++)
            std::cout << arr[y * width + x] << "\t";
        std::cout << "\n";
    }
}

void test_uniform_image() {
    std::cout << "\n=== Test 1: Uniform Image ===" << std::endl;
    const int W = 5, H = 5;
    uint8_t src[W * H];
    int16_t Gx[W * H], Gy[W * H];

    for (int i = 0; i < W * H; i++) src[i] = 100;
    sobel_gradient(src, Gx, Gy, W, H);

    bool passed = true;
    for (int y = 1; y < H-1; y++)
        for (int x = 1; x < W-1; x++)
            if (Gx[y*W+x] != 0 || Gy[y*W+x] != 0) passed = false;

    std::cout << "Test 1: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_vertical_edge() {
    std::cout << "\n=== Test 2: Vertical Edge ===" << std::endl;
    const int W = 6, H = 5;
    uint8_t src[W * H];
    int16_t Gx[W * H], Gy[W * H];

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            src[y*W+x] = (x < W/2) ? 0 : 255;

    sobel_gradient(src, Gx, Gy, W, H);

    bool passed = false;
    for (int y = 1; y < H-1; y++)
        if (abs(Gx[y*W + W/2]) > 100) passed = true;

    std::cout << "Test 2: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_horizontal_edge() {
    std::cout << "\n=== Test 3: Horizontal Edge ===" << std::endl;
    const int W = 5, H = 6;
    uint8_t src[W * H];
    int16_t Gx[W * H], Gy[W * H];

    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            src[y*W+x] = (y < H/2) ? 0 : 255;

    sobel_gradient(src, Gx, Gy, W, H);

    bool passed = false;
    for (int x = 1; x < W-1; x++)
        if (abs(Gy[(H/2)*W + x]) > 100) passed = true;

    std::cout << "Test 3: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

int main() {
    std::cout << "=== Sobel Tests ===" << std::endl;
    test_uniform_image();
    test_vertical_edge();
    test_horizontal_edge();
    return 0;
}