#include <iostream>
#include <cmath>
#include <cstdint>
#include "../include/magnitude.h"

void test_l1_basic() {
    std::cout << "\n=== Test 1: L1 Basic ===" << std::endl;
    int16_t gx[1] = {3};
    int16_t gy[1] = {4};
    uint8_t mag[1];

    magnitude_l1(gx, gy, mag, 1, 1);

    // single pixel, max = raw value, so normalized = 255
    bool passed = (mag[0] == 255);
    std::cout << "Result: " << (int)mag[0] << std::endl;
    std::cout << "Test 1: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_l2_basic() {
    std::cout << "\n=== Test 2: L2 Basic ===" << std::endl;
    int16_t gx[1] = {3};
    int16_t gy[1] = {4};
    uint8_t mag[1];

    magnitude_l2(gx, gy, mag, 1, 1);

    bool passed = (mag[0] == 255);
    std::cout << "Result: " << (int)mag[0] << std::endl;
    std::cout << "Test 2: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_zero_image() {
    std::cout << "\n=== Test 3: All Zero ===" << std::endl;
    int16_t gx[4] = {0,0,0,0};
    int16_t gy[4] = {0,0,0,0};
    uint8_t mag[4];

    magnitude_l1(gx, gy, mag, 2, 2);

    bool passed = true;
    for (int i = 0; i < 4; i++)
        if (mag[i] != 0) passed = false;

    std::cout << "Test 3: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_normalization() {
    std::cout << "\n=== Test 4: Normalization ===" << std::endl;
    // Two pixels: one weak, one strong edge
    int16_t gx[2] = {100, 500};
    int16_t gy[2] = {0, 0};
    uint8_t mag[2];

    magnitude_l1(gx, gy, mag, 2, 1);

    // Strongest pixel should be 255, weaker should be proportional
    bool passed = (mag[1] == 255) && (mag[0] < mag[1]);
    std::cout << "mag[0]=" << (int)mag[0] << " mag[1]=" << (int)mag[1] << std::endl;
    std::cout << "Test 4: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

int main() {
    std::cout << "=== Magnitude Tests ===" << std::endl;
    test_l1_basic();
    test_l2_basic();
    test_zero_image();
    test_normalization();
    return 0;
}