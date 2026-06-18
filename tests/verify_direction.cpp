#include <iostream>
#include <cstdint>
#include "../include/direction.h"

void test_horizontal_dominant() {
    std::cout << "\n=== Test 1: Horizontal Dominant (ax > ay) ===" << std::endl;
    int16_t gx[1] = {100};
    int16_t gy[1] = {10};
    uint8_t dir[1];

    direction_compute(gx, gy, dir, 1, 1);

    bool passed = (dir[0] == 0);
    std::cout << "Result: " << (int)dir[0] << " degrees" << std::endl;
    std::cout << "Test 1: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_vertical_dominant() {
    std::cout << "\n=== Test 2: Vertical Dominant (ay > ax) ===" << std::endl;
    int16_t gx[1] = {10};
    int16_t gy[1] = {100};
    uint8_t dir[1];

    direction_compute(gx, gy, dir, 1, 1);

    bool passed = (dir[0] == 90);
    std::cout << "Result: " << (int)dir[0] << " degrees" << std::endl;
    std::cout << "Test 2: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_diagonal_45() {
    std::cout << "\n=== Test 3: Diagonal 45 (same sign) ===" << std::endl;
    int16_t gx[1] = {50};
    int16_t gy[1] = {50};
    uint8_t dir[1];

    direction_compute(gx, gy, dir, 1, 1);

    bool passed = (dir[0] == 45);
    std::cout << "Result: " << (int)dir[0] << " degrees" << std::endl;
    std::cout << "Test 3: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_diagonal_135() {
    std::cout << "\n=== Test 4: Diagonal 135 (opposite sign) ===" << std::endl;
    int16_t gx[1] = {50};
    int16_t gy[1] = {-50};
    uint8_t dir[1];

    direction_compute(gx, gy, dir, 1, 1);

    bool passed = (dir[0] == 135);
    std::cout << "Result: " << (int)dir[0] << " degrees" << std::endl;
    std::cout << "Test 4: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

void test_zero_gradient() {
    std::cout << "\n=== Test 5: Zero Gradient (tie, same sign) ===" << std::endl;
    int16_t gx[1] = {0};
    int16_t gy[1] = {0};
    uint8_t dir[1];

    direction_compute(gx, gy, dir, 1, 1);

    // ax = ay = 0, both >= 0 → 45 by current logic
    bool passed = (dir[0] == 45);
    std::cout << "Result: " << (int)dir[0] << " degrees" << std::endl;
    std::cout << "Test 5: " << (passed ? "PASSED " : "FAILED ") << std::endl;
}

int main() {
    std::cout << "=== Direction Tests ===" << std::endl;
    test_horizontal_dominant();
    test_vertical_dominant();
    test_diagonal_45();
    test_diagonal_135();
    test_zero_gradient();
    return 0;
}