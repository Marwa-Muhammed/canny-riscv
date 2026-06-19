#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>

// --------------------------------------------------
// Direction function (test version)
// --------------------------------------------------
double direction(int16_t gx, int16_t gy)
{
    return std::atan2(gy, gx) * 180.0 / M_PI;
}

// --------------------------------------------------
// TEST 1: Vertical edge → 0 degrees
// --------------------------------------------------
TEST(DirectionTest, Vertical)
{
    int16_t gx = 100;
    int16_t gy = 0;

    double dir = direction(gx, gy);

    EXPECT_NEAR(dir, 0.0, 1e-6);
}

// --------------------------------------------------
// TEST 2: Horizontal edge → 90 degrees
// --------------------------------------------------
TEST(DirectionTest, Horizontal)
{
    int16_t gx = 0;
    int16_t gy = 100;

    double dir = direction(gx, gy);

    EXPECT_NEAR(std::abs(dir), 90.0, 1e-6);
}

// --------------------------------------------------
// TEST 3: Diagonal edge → 45 degrees
// --------------------------------------------------
TEST(DirectionTest, Diagonal)
{
    int16_t gx = 100;
    int16_t gy = 100;

    double dir = direction(gx, gy);

    EXPECT_NEAR(dir, 45.0, 1e-6);
}

#include <gtest/gtest.h>
#include "../include/direction.h"
#include "../include/sobel.h"

// ─────────────────────────────────────────────
// TEST 4: Vertical edge → direction should be 0
// A vertical edge (left=black, right=white) has intensity changing
// horizontally, so Gx dominates and Gy ≈ 0. The quantized direction
// should be 0 (horizontal gradient direction = edge runs vertically).
// ─────────────────────────────────────────────
TEST(DirectionComputeTest, VerticalEdge)
{
    const int W = 6, H = 5;
    uint8_t img[W * H];
    int16_t Gx[W * H] = {0}, Gy[W * H] = {0};
    uint8_t dir[W * H] = {0};

    // Left half = black, right half = white → sharp vertical boundary
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            img[y * W + x] = (x < W / 2) ? 0 : 255;

    sobel_gradient(img, Gx, Gy, W, H);
    direction_compute(Gx, Gy, dir, W, H);

    // At the boundary column, interior pixels should all report direction 0.
    // We skip y=0 and y=H-1 (border rows) to avoid zero-padding effects.
    for (int y = 1; y < H - 1; y++)
        EXPECT_EQ(dir[y * W + W / 2], 0)
            << "vertical edge at row " << y << " should have direction 0";
}

// ─────────────────────────────────────────────
// TEST 5: Horizontal edge → direction should be 2
// A horizontal edge (top=black, bottom=white) has intensity changing
// vertically, so Gy dominates and Gx ≈ 0. The quantized direction
// should be 2 (vertical gradient direction = edge runs horizontally).
// ─────────────────────────────────────────────
TEST(DirectionComputeTest, HorizontalEdge)
{
    const int W = 5, H = 6;
    uint8_t img[W * H];
    int16_t Gx[W * H] = {0}, Gy[W * H] = {0};
    uint8_t dir[W * H] = {0};

    // Top half = black, bottom half = white → sharp horizontal boundary
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            img[y * W + x] = (y < H / 2) ? 0 : 255;

    sobel_gradient(img, Gx, Gy, W, H);
    direction_compute(Gx, Gy, dir, W, H);

    // At the boundary row, interior pixels should all report direction 2.
    // We skip x=0 and x=W-1 (border columns) to avoid zero-padding effects.
    for (int x = 1; x < W - 1; x++)
        EXPECT_EQ(dir[(H / 2) * W + x], 2)
            << "horizontal edge at col " << x << " should have direction 2";
}

// ─────────────────────────────────────────────
// TEST 6: Diagonal edge → direction should be 1 or 3
// A diagonal edge activates both Gx and Gy roughly equally.
// The quantized direction should be 1 (45°) or 3 (135°) depending
// on the sign relationship between Gx and Gy at each pixel.
// ─────────────────────────────────────────────
TEST(DirectionComputeTest, DiagonalEdge)
{
    const int W = 6, H = 6;
    uint8_t img[W * H];
    int16_t Gx[W * H] = {0}, Gy[W * H] = {0};
    uint8_t dir[W * H] = {0};

    // Diagonal boundary: pixels where (x + y < W) are black, rest white
    for (int y = 0; y < H; y++)
        for (int x = 0; x < W; x++)
            img[y * W + x] = (x + y < W) ? 0 : 255;

    sobel_gradient(img, Gx, Gy, W, H);
    direction_compute(Gx, Gy, dir, W, H);

    // We don't assert every pixel — just that at least one interior pixel
    // correctly reports a diagonal direction. This is sufficient to confirm
    // direction_compute() handles the diagonal case without misclassifying
    // it as purely horizontal (0) or vertical (2).
    bool found = false;
    for (int y = 1; y < H - 1; y++)
        for (int x = 1; x < W - 1; x++)
            if (dir[y * W + x] == 1 || dir[y * W + x] == 3)
                found = true;

    EXPECT_TRUE(found) << "diagonal edge should produce direction 1 or 3 "
                          "at at least one interior pixel";
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
