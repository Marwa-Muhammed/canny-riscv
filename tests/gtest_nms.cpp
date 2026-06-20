#include <gtest/gtest.h>
#include "nms.h"
#include <vector>

// ── Test 1: Uniform magnitude → interior pixels survive ───────────────────
// Rationale (UPDATED): NMS now compares with >= instead of strict >.
// A pixel that is equal to its neighbours along the gradient direction is
// treated as a local maximum (tie) and is KEPT, not suppressed. On a fully
// uniform magnitude image, every interior pixel equals all its neighbours,
// so every interior pixel survives. Border pixels still suppress to 0,
// since that's caused by boundary/zero-padding handling, not by the
// strict-vs-non-strict comparison change.
//
// Previously (with strict >): no pixel was STRICTLY greater than its
// neighbours, so everything suppressed to 0. That assumption no longer
// holds after the >= fix, so this test's expected output changes from
// "all zero" to "interior pixels keep their original magnitude (128),
// border pixels are zero."
TEST(NMSTest, UniformMagnitudeInteriorPixelsSurvive) {
    const int W = 10, H = 10;
    std::vector<uint8_t> mag(W * H, 128);
    std::vector<uint8_t> dir(W * H, 0);
    std::vector<uint8_t> out(W * H, 255);  // pre-fill non-zero to catch misses

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    for (int r = 0; r < H; r++) {
        for (int c = 0; c < W; c++) {
            int i = r * W + c;
            bool is_border = (r == 0 || r == H - 1 || c == 0 || c == W - 1);
            if (is_border)
                EXPECT_EQ(out[i], 0) << "border pixel (" << r << "," << c << ") should be 0";
            else
                EXPECT_EQ(out[i], 128) << "interior pixel (" << r << "," << c << ") should survive with >=";
        }
    }
}

// ── Test 2: All-black magnitude → all-black output ───────────────────────
TEST(NMSTest, AllZeroMagnitudeProducesAllZeros) {
    const int W = 8, H = 8;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 2);
    std::vector<uint8_t> out(W * H, 99);   // pre-fill to detect bugs

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    for (int i = 0; i < W * H; i++)
        EXPECT_EQ(out[i], 0);
}

// ── Test 3: Border pixels are always zero ─────────────────────────────────
// Even if magnitude is high, border pixels must be suppressed
TEST(NMSTest, BorderPixelsAlwaysZero) {
    const int W = 8, H = 8;
    std::vector<uint8_t> mag(W * H, 200);  // high magnitude everywhere
    std::vector<uint8_t> dir(W * H, 0);
    std::vector<uint8_t> out(W * H, 0);

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    for (int c = 0; c < W; c++) {
        EXPECT_EQ(out[0 * W + c], 0);        // top row
        EXPECT_EQ(out[(H-1) * W + c], 0);    // bottom row
    }
    for (int r = 0; r < H; r++) {
        EXPECT_EQ(out[r * W + 0], 0);        // left column
        EXPECT_EQ(out[r * W + (W-1)], 0);    // right column
    }
}

// ── Test 4: Single isolated peak survives (dir = 0, horizontal) ──────────
// Peak neighbours are 0, which are < 100, so peak must survive
TEST(NMSTest, SinglePeakSurvives_Dir0) {
    const int W = 5, H = 5;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 0);
    std::vector<uint8_t> out(W * H, 0);

    mag[2 * W + 2] = 100;  // single peak at centre

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[2 * W + 2], 100);  // peak should survive

    for (int i = 0; i < W * H; i++)
        if (i != 2 * W + 2)
            EXPECT_EQ(out[i], 0);
}

// ── Test 5: Non-maximum is suppressed (dir = 0) ───────────────────────────
// Row of [50, 80, 90]: middle pixel 80 is less than right neighbour 90
// → 80 should be suppressed, 90 should survive
TEST(NMSTest, NonMaximumIsSuppressed_Dir0) {
    const int W = 5, H = 3;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 0);
    std::vector<uint8_t> out(W * H, 0);

    int r = 1;  // interior row
    mag[r * W + 1] = 50;
    mag[r * W + 2] = 80;   // non-maximum: left=50, right=90
    mag[r * W + 3] = 90;

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[r * W + 2], 0)  << "non-maximum (80) must be suppressed";
    EXPECT_EQ(out[r * W + 3], 90) << "local max (90) must survive";
}

// ── Test 6: Vertical gradient (dir = 2, 90°) ─────────────────────────────
// Rationale (UPDATED): with >=, a uniform column of equal-magnitude pixels
// ties with its neighbours along the gradient direction rather than losing
// to a strictly-greater one. A tie is now kept, so the entire plateau
// survives NMS instead of being wiped to 0.
//
// Previously (with strict >): top=200, bottom=200, center=200 meant no
// pixel was STRICTLY greater than its neighbours, so the whole column
// suppressed to 0. That is no longer correct behavior after the >= fix.
TEST(NMSTest, VerticalPlateauSurvives_Dir2) {
    const int W = 5, H = 5;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 2);
    std::vector<uint8_t> out(W * H, 0);

    for (int r = 0; r < H; r++)
        mag[r * W + 2] = 200;   // uniform centre column

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    // Interior pixels of centre column: top=200, bottom=200 → tie under >=, survives
    for (int r = 1; r < H - 1; r++)
        EXPECT_EQ(out[r * W + 2], 200) << "plateau row " << r << " should survive with >=";
}

// ── Test 7: Diagonal peak survives (dir = 1, 45°) ────────────────────────
TEST(NMSTest, DiagonalPeakSurvives_Dir1) {
    const int W = 7, H = 7;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 1);
    std::vector<uint8_t> out(W * H, 0);

    mag[3 * W + 3] = 150;   // peak
    mag[2 * W + 4] = 50;    // top-right neighbour (smaller)
    mag[4 * W + 2] = 50;    // bottom-left neighbour (smaller)

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[3 * W + 3], 150) << "diagonal peak (dir=1) should survive";
    EXPECT_EQ(out[2 * W + 4],   0) << "smaller neighbour should be suppressed";
    EXPECT_EQ(out[4 * W + 2],   0) << "smaller neighbour should be suppressed";
}

// ── Test 8: Diagonal peak survives (dir = 3, 135°) ───────────────────────
TEST(NMSTest, DiagonalPeakSurvives_Dir3) {
    const int W = 7, H = 7;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 3);
    std::vector<uint8_t> out(W * H, 0);

    mag[3 * W + 3] = 200;   // peak
    mag[2 * W + 2] = 100;   // top-left neighbour (smaller)
    mag[4 * W + 4] = 100;   // bottom-right neighbour (smaller)

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[3 * W + 3], 200) << "diagonal peak (dir=3) should survive";
    EXPECT_EQ(out[2 * W + 2],   0);
    EXPECT_EQ(out[4 * W + 4],   0);
}

// ── Test 9: uint16_t magnitude template instantiation ────────────────────
// Ensures the template compiles and runs with wider magnitude type
TEST(NMSTest, Uint16MagnitudeInstantiation) {
    const int W = 5, H = 5;
    std::vector<uint16_t> mag(W * H, 0);
    std::vector<uint8_t>  dir(W * H, 0);
    std::vector<uint8_t>  out(W * H, 0);

    mag[2 * W + 2] = 1000;  // single peak in 16-bit range

    nonMaxSuppression<uint16_t, uint8_t, uint8_t>(
        mag.data(), dir.data(), out.data(), W, H);

    EXPECT_GT(out[2 * W + 2], 0u) << "16-bit peak should produce non-zero output";
}

// ── Test 10: Non-power-of-two image size (100 × 75) ──────────────────────
// Catches off-by-one errors on non-standard dimensions
TEST(NMSTest, NonPowerOfTwoImageSize) {
    const int W = 100, H = 75;
    std::vector<uint8_t> mag(W * H, 0);
    std::vector<uint8_t> dir(W * H, 0);
    std::vector<uint8_t> out(W * H, 255);  // pre-fill to detect border bugs

    const int r = 37, c = 49;
    mag[r * W + c] = 200;   // isolated peak well inside the image

    nms_u8(mag.data(), dir.data(), out.data(), W, H);

    EXPECT_EQ(out[r * W + c], 200) << "peak in 100x75 image should survive";

    // Check all border pixels are zero
    for (int col = 0; col < W; col++) {
        EXPECT_EQ(out[0 * W + col],     0) << "top border col " << col;
        EXPECT_EQ(out[(H-1)*W + col],   0) << "bottom border col " << col;
    }
    for (int row = 0; row < H; row++) {
        EXPECT_EQ(out[row * W + 0],     0) << "left border row " << row;
        EXPECT_EQ(out[row * W + (W-1)], 0) << "right border row " << row;
    }
}

int main(int argc, char** argv) {
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
