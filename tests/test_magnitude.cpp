#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>

// --------------------------------------------------
// Magnitude function (test version)
// --------------------------------------------------
double magnitude(int16_t gx, int16_t gy)
{
    return std::sqrt(gx * gx + gy * gy);
}

// --------------------------------------------------
// TEST 1: Horizontal edge
// --------------------------------------------------
TEST(MagnitudeTest, Horizontal)
{
    int16_t gx = 0;
    int16_t gy = 100;

    double mag = magnitude(gx, gy);

    EXPECT_NEAR(mag, 100.0, 1e-6);
}

// --------------------------------------------------
// TEST 2: Vertical edge
// --------------------------------------------------
TEST(MagnitudeTest, Vertical)
{
    int16_t gx = 100;
    int16_t gy = 0;

    double mag = magnitude(gx, gy);

    EXPECT_NEAR(mag, 100.0, 1e-6);
}

// --------------------------------------------------
// TEST 3: Diagonal edge (3-4-5 triangle)
// --------------------------------------------------
TEST(MagnitudeTest, Diagonal)
{
    int16_t gx = 3;
    int16_t gy = 4;

    double mag = magnitude(gx, gy);

    EXPECT_NEAR(mag, 5.0, 1e-6);
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}