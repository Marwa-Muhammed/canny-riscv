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

// --------------------------------------------------
// MAIN
// --------------------------------------------------
int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}