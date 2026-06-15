#include <gtest/gtest.h>
#include <cstdint>
#include <cmath>

#include "../include/sobel.h"

// ─────────────────────────────────────────────
// TEST 1: Uniform image → all gradients must be 0
// ─────────────────────────────────────────────
TEST(SobelTest, UniformImage)
{
    const int W = 5, H = 5;

    uint8_t img[W * H];
    int16_t Gx[W * H] = {0};
    int16_t Gy[W * H] = {0};

    // Fill uniform image
    for (int i = 0; i < W * H; i++)
        img[i] = 100;

    sobel_gradient(img, Gx, Gy, W, H);

    // Only check INTERIOR pixels (important for zero-padding behavior)
    for (int y = 1; y < H - 1; y++)
    {
        for (int x = 1; x < W - 1; x++)
        {
            int idx = y * W + x;

            EXPECT_EQ(Gx[idx], 0);
            EXPECT_EQ(Gy[idx], 0);
        }
    }
}

// ─────────────────────────────────────────────
// TEST 2: Vertical edge → strong Gx response
// ─────────────────────────────────────────────
TEST(SobelTest, VerticalEdge)
{
    const int W = 6, H = 5;

    uint8_t img[W * H];
    int16_t Gx[W * H] = {0};
    int16_t Gy[W * H] = {0};

    // Left = 0, Right = 255
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            img[y * W + x] = (x < W / 2) ? 0 : 255;
        }
    }

    sobel_gradient(img, Gx, Gy, W, H);

    bool found = false;

    for (int y = 1; y < H - 1; y++)
    {
        int gx = std::abs(Gx[y * W + W / 2]);
        if (gx > 50)
            found = true;
    }

    EXPECT_TRUE(found);
}

// ─────────────────────────────────────────────
// TEST 3: Horizontal edge → strong Gy response
// ─────────────────────────────────────────────
TEST(SobelTest, HorizontalEdge)
{
    const int W = 5, H = 6;

    uint8_t img[W * H];
    int16_t Gx[W * H] = {0};
    int16_t Gy[W * H] = {0};

    // Top = 0, Bottom = 255
    for (int y = 0; y < H; y++)
    {
        for (int x = 0; x < W; x++)
        {
            img[y * W + x] = (y < H / 2) ? 0 : 255;
        }
    }

    sobel_gradient(img, Gx, Gy, W, H);

    bool found = false;

    for (int x = 1; x < W - 1; x++)
    {
        int gy = std::abs(Gy[(H / 2) * W + x]);
        if (gy > 50)
            found = true;
    }

    EXPECT_TRUE(found);
}

// ─────────────────────────────────────────────
// MAIN (required for standalone gtest build)
// ─────────────────────────────────────────────
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
