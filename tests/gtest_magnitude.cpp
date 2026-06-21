#include <gtest/gtest.h>
#include <cmath>
#include <cstdint>

// --------------------------------------------------
// Magnitude function (test version)
// --------------------------------------------------
double magnitude(int16_t gx, int16_t gy)
{
    return std::sqrt((double)gx * gx + (double)gy * gy);
}

// L1 magnitude helper
double magnitude_l1(int16_t gx, int16_t gy)
{
    return abs(gx) + abs(gy);
}

// --------------------------------------------------
// TEST 1: Horizontal edge
// --------------------------------------------------
TEST(MagnitudeTest, Horizontal)
{
    int16_t gx = 0;
    int16_t gy = 100;
    EXPECT_NEAR(magnitude(gx, gy), 100.0, 1e-6);
}

// --------------------------------------------------
// TEST 2: Vertical edge
// --------------------------------------------------
TEST(MagnitudeTest, Vertical)
{
    int16_t gx = 100;
    int16_t gy = 0;
    EXPECT_NEAR(magnitude(gx, gy), 100.0, 1e-6);
}

// --------------------------------------------------
// TEST 3: Diagonal edge (3-4-5 triangle)
// --------------------------------------------------
TEST(MagnitudeTest, Diagonal)
{
    int16_t gx = 3;
    int16_t gy = 4;
    EXPECT_NEAR(magnitude(gx, gy), 5.0, 1e-6);
}

// --------------------------------------------------
// TEST 4: Zero magnitude
// --------------------------------------------------
TEST(MagnitudeTest, Zero)
{
    int16_t gx = 0;
    int16_t gy = 0;
    EXPECT_NEAR(magnitude(gx, gy), 0.0, 1e-6);
}

// --------------------------------------------------
// TEST 5: Negative values
// --------------------------------------------------
TEST(MagnitudeTest, Negative)
{
    int16_t gx = -3;
    int16_t gy = -4;
    // magnitude should be same as positive
    EXPECT_NEAR(magnitude(gx, gy), 5.0, 1e-6);
}

// --------------------------------------------------
// TEST 6: Maximum Sobel value (FIXED)
// sqrt(1020² + 1020²) = 1442.497...
// --------------------------------------------------
TEST(MagnitudeTest, MaxSobelValue)
{
    int16_t gx = 1020;
    int16_t gy = 1020;
    EXPECT_NEAR(magnitude(gx, gy), 1442.5, 0.5);
}

// --------------------------------------------------
// TEST 7: L1 always >= L2
// --------------------------------------------------
TEST(MagnitudeTest, L1vsL2)
{
    int16_t gx = 3;
    int16_t gy = 4;
    double l1 = magnitude_l1(gx, gy);  // = 7
    double l2 = magnitude(gx, gy);     // = 5
    EXPECT_GE(l1, l2);
    EXPECT_GT(l1, l2);
}

// --------------------------------------------------
// MAIN
// --------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}