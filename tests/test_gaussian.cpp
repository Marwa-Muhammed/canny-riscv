#include <gtest/gtest.h>
#include "gaussian.h"
#include <vector>

// Test 1: Blurring a uniform image should produce the same uniform image
TEST(GaussianTest, UniformImageInvariant) {
    const int W = 64, H = 64;
    std::vector<uint8_t> src(W * H, 128);  // all pixels = 128
    std::vector<uint8_t> dst(W * H, 0);

    gaussian_blur(src.data(), dst.data(), W, H);

    // Check interior pixels only (avoid border effects)
    for (int y = 2; y < H - 2; y++) {
        for (int x = 2; x < W - 2; x++) {
            int val = dst[y * W + x];
            EXPECT_NEAR(val, 128, 1);  // allow ±1 for rounding
        }
    }
}

// Test 2: Blurring an all-black image should produce all-black
TEST(GaussianTest, AllBlackImage) {
    const int W = 64, H = 64;
    std::vector<uint8_t> src(W * H, 0);
    std::vector<uint8_t> dst(W * H, 255);

    gaussian_blur(src.data(), dst.data(), W, H);

    for (int i = 0; i < W * H; i++) {
        EXPECT_EQ(dst[i], 0);
    }
}

// Test 3: Impulse response - single bright pixel should spread to neighbors
TEST(GaussianTest, ImpulseResponse) {
    const int W = 16, H = 16;
    std::vector<uint8_t> src(W * H, 0);
    std::vector<uint8_t> dst(W * H, 0);

    // Single bright pixel in the center
    src[8 * W + 8] = 255;

    gaussian_blur(src.data(), dst.data(), W, H);

    // Center should still be brightest
    EXPECT_GT(dst[8 * W + 8], dst[7 * W + 8]);  // center > neighbor
    EXPECT_GT(dst[8 * W + 8], 0);                // center > 0
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
