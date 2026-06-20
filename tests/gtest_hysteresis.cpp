#include <gtest/gtest.h>
#include "hysteresis.h"

static uint8_t* make_image(int size, uint8_t fill_value) {
    uint8_t* img = new uint8_t[size];
    for (int i = 0; i < size; i++) img[i] = fill_value;
    return img;
}

// ============================================================
// TEST 1: All strong edges stay strong
// Strong pixels (255) should never be suppressed.
// ============================================================
TEST(HysteresisTest, AllStrongStaysStrong) {
    const int W = 5, H = 5;
    uint8_t* input  = make_image(W * H, 255);
    uint8_t* output = new uint8_t[W * H];

    hysteresis(input, output, W, H);

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(output[i], 255) << "at pixel " << i;

    delete[] input;
    delete[] output;
}

// ============================================================
// TEST 2: All weak edges with no strong neighbor -> suppressed
// Isolated weak pixels must be suppressed to 0.
// ============================================================
TEST(HysteresisTest, AllWeakNoStrongNeighbor) {
    const int W = 5, H = 5;
    uint8_t* input  = make_image(W * H, 128);
    uint8_t* output = new uint8_t[W * H];

    hysteresis(input, output, W, H);

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(output[i], 0) << "at pixel " << i;

    delete[] input;
    delete[] output;
}

// ============================================================
// TEST 3: All non-edges stay zero
// ============================================================
TEST(HysteresisTest, AllZeroStaysZero) {
    const int W = 5, H = 5;
    uint8_t* input  = make_image(W * H, 0);
    uint8_t* output = new uint8_t[W * H];

    hysteresis(input, output, W, H);

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(output[i], 0) << "at pixel " << i;

    delete[] input;
    delete[] output;
}

// ============================================================
// TEST 4: Weak pixel adjacent to strong -> promoted to 255
// A single weak pixel next to a strong one must survive.
// ============================================================
TEST(HysteresisTest, WeakAdjacentToStrongPromoted) {
    const int W = 3, H = 3;
    uint8_t input[9] = {
        0,   0,   0,
        0,   128, 255,   // center=weak, right=strong
        0,   0,   0
    };
    uint8_t output[9] = {};

    hysteresis(input, output, W, H);

    EXPECT_EQ(output[4], 255);  // weak at center promoted
    EXPECT_EQ(output[5], 255);  // strong stays strong
}

// ============================================================
// TEST 5: Weak pixel NOT adjacent to strong -> suppressed
// ============================================================
TEST(HysteresisTest, WeakNotAdjacentToStrong) {
    const int W = 5, H = 3;
    uint8_t input[15] = {
        0,   0,   0,   0,   0,
        255, 0,   0,   0,   128,  // strong at left, weak at right, gap in between
        0,   0,   0,   0,   0
    };
    uint8_t output[15] = {};

    hysteresis(input, output, W, H);

    EXPECT_EQ(output[9],  0);    // weak suppressed
    EXPECT_EQ(output[5],  255);  // strong unchanged
}
// ============================================================
// TEST 6: Chain propagation
// Weak -> Weak -> Strong: the whole chain must be promoted.
// This tests that hysteresis iterates until convergence.
// ============================================================
TEST(HysteresisTest, ChainPropagation) {
    const int W = 5, H = 3;
    uint8_t input[15] = {
        0,   0,   0,   0,   0,
        255, 128, 128, 128, 0,   // strong at (1,0), weak chain at (1,1),(1,2),(1,3)
        0,   0,   0,   0,   0
    };
    uint8_t output[15] = {};

    hysteresis(input, output, W, H);

    EXPECT_EQ(output[5], 255);  // strong
    EXPECT_EQ(output[6], 255);  // promoted
    EXPECT_EQ(output[7], 255);  // promoted
    EXPECT_EQ(output[8], 255);  // promoted
    EXPECT_EQ(output[9], 0);    // non-edge stays 0
}

// ============================================================
// TEST 7: Non-power-of-two image size
// Forces strip-mining edge cases and boundary handling.
// ============================================================
TEST(HysteresisTest, NonPowerOfTwoSize) {
    const int W = 7, H = 5;
    uint8_t* input  = make_image(W * H, 128);
    uint8_t* output = new uint8_t[W * H];

    // Place one strong pixel in the interior
    input[2 * W + 3] = 255;

    hysteresis(input, output, W, H);

    // Strong pixel survives
    EXPECT_EQ(output[2 * W + 3], 255);
    // Its direct neighbors (weak) get promoted
    EXPECT_EQ(output[2 * W + 4], 255);
    EXPECT_EQ(output[2 * W + 2], 255);

    delete[] input;
    delete[] output;
}