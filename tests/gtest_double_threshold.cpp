#include <gtest/gtest.h>
#include "double_threshold.h"

// ============================================================
// Helper: allocate and fill an image with a constant value
// ============================================================
static uint8_t* make_image(int size, uint8_t fill_value) {
    uint8_t* img = new uint8_t[size];
    for (int i = 0; i < size; i++) img[i] = fill_value;
    return img;
}

// ============================================================
// TEST 1: All pixels below low threshold -> all suppressed (0)
// If every magnitude value is below low_thresh, nothing is an edge.
// ============================================================
TEST(DoubleThreshold, AllBelowLow) {
    const int W = 10, H = 10;
    uint8_t* mag = make_image(W * H, 20);   // all pixels = 20
    uint8_t* out = new uint8_t[W * H];

    double_threshold(mag, out, W, H, 50, 150); // low=50, high=150

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(out[i], NO_EDGE) << "at pixel " << i;

    delete[] mag;
    delete[] out;
}

// ============================================================
// TEST 2: All pixels above high threshold -> all strong edges (255)
// If every magnitude value exceeds high_thresh, all are strong edges.
// ============================================================
TEST(DoubleThreshold, AllAboveHigh) {
    const int W = 10, H = 10;
    uint8_t* mag = make_image(W * H, 200);  // all pixels = 200
    uint8_t* out = new uint8_t[W * H];

    double_threshold(mag, out, W, H, 50, 150);

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(out[i], STRONG_EDGE) << "at pixel " << i;

    delete[] mag;
    delete[] out;
}

// ============================================================
// TEST 3: All pixels in between -> all weak edges (128)
// Pixels between low and high are uncertain candidates.
// ============================================================
TEST(DoubleThreshold, AllInBetween) {
    const int W = 10, H = 10;
    uint8_t* mag = make_image(W * H, 100);  // all pixels = 100, between 50 and 150
    uint8_t* out = new uint8_t[W * H];

    double_threshold(mag, out, W, H, 50, 150);

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(out[i], WEAK_EDGE) << "at pixel " << i;

    delete[] mag;
    delete[] out;
}

// ============================================================
// TEST 4: Mixed pixel values -> correct classification per pixel
// Manually set specific pixels and verify each is classified correctly.
// ============================================================
TEST(DoubleThreshold, MixedPixels) {
    const int W = 3, H = 1; // single row of 3 pixels for simplicity
    uint8_t mag[3] = {20, 100, 200};  // below low, in between, above high
    uint8_t out[3] = {0, 0, 0};

    double_threshold(mag, out, W, H, 50, 150);

    EXPECT_EQ(out[0], NO_EDGE);     // 20 < 50  -> suppressed
    EXPECT_EQ(out[1], WEAK_EDGE);   // 50 <= 100 < 150 -> weak
    EXPECT_EQ(out[2], STRONG_EDGE); // 200 >= 150 -> strong
}

// ============================================================
// TEST 5: Pixels exactly on threshold boundaries
// Boundary values must fall into the correct category.
// ============================================================
TEST(DoubleThreshold, BoundaryValues) {
    const int W = 4, H = 1;
    uint8_t mag[4] = {49, 50, 149, 150}; // just below low, at low, just below high, at high
    uint8_t out[4] = {0, 0, 0, 0};

    double_threshold(mag, out, W, H, 50, 150);

    EXPECT_EQ(out[0], NO_EDGE);     // 49 < 50   -> suppressed
    EXPECT_EQ(out[1], WEAK_EDGE);   // 50 >= 50  -> weak
    EXPECT_EQ(out[2], WEAK_EDGE);   // 149 < 150 -> weak
    EXPECT_EQ(out[3], STRONG_EDGE); // 150 >= 150 -> strong
}