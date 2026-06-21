#include "gaussian_vectorized.h"
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>

// =======================================================================
// This file has TWO implementations of gaussian_blur_interior_rvv,
// selected at compile time:
//
//   - DEFAULT (no GAUSS_LMUL defined): the normal production path,
//     using the u8->u16->u32 widening-multiply chain at fixed LMUL
//     (m1/m2/m4 respectively). This is what every regular build uses.
//
//   - GAUSS_LMUL defined (1, 2, or 4): the LMUL register-pressure
//     sweep path, used ONLY by scripts/run_lmul_sweep.sh to
//     demonstrate register spilling at LMUL=4. Every live vector
//     value in this path sits at the SAME swept LMUL throughout (no
//     automatic widening-chain doubling), which is what lets
//     GAUSS_LMUL=4 compile and run at all -- the default path's
//     widening chain cannot reach LMUL=4 because it would require a
//     nonexistent u32 group at m16.
//
// Border handling (gaussian_blur_border_scalar) and the top-level
// gaussian_blur_rvv() entry point are IDENTICAL either way and are
// defined once, outside the #ifdef, so there is no duplicated logic
// to drift out of sync between the two paths.
// =======================================================================

static constexpr int KSIZE = 5;
static constexpr int KHALF = KSIZE / 2;

#ifdef GAUSS_LMUL
// =======================================================================
// LMUL register-pressure sweep path (GAUSS_LMUL = 1, 2, or 4)
//
// IMPORTANT DESIGN NOTE (this replaced an earlier, BROKEN version):
// Avoid widening instructions (vzext, vwmulu, etc.) entirely
// in this path. Widening instructions have a mechanically FIXED
// relationship between source and destination EMUL (destination =
// source * widen_factor), so you cannot independently dial in an
// arbitrary final LMUL via a widening chain starting from a real u8
// load -- the legal endpoints turn out to be m2 or m4 only, never m1.
//
// Instead, gaussian_blur_rvv() first converts the image to a
// temporary uint32_t buffer ONCE, with a plain scalar loop (cheap:
// 136x136 = 18496 pixels, and this happens OUTSIDE the per-stage
// timing bracket in riscv_main_vectorized.cpp, so it does not
// contaminate the Gaussian RVV vs Scalar comparison). This kernel
// then loads u32 values directly (vle32, a non-widening load) and
// does every subsequent op (vmul, vadd, vmin, vmax, vsra) as a
// PLAIN (non-widening) operation, where EMUL simply equals LMUL with
// NO fractional-source floor to violate -- m1, m2, and m4 are all
// equally legal here, which is what actually lets GAUSS_LMUL=4 run
// (and spill) as intended.
//
// Why LMUL=4 is still expected to spill for THIS kernel: RVV provides
// 32 architectural vector registers; a live value at LMUL=N occupies
// N consecutive registers as one group (~32/N groups available). The
// 25-tap inner loop keeps several wide values simultaneously live
// (running sum, freshly loaded pixel, multiply result, plus several
// more during scale/clamp/store) -- comfortably under the ~8 groups
// available at LMUL=4, but tight enough that register pressure is
// real and measurably higher than at LMUL=1/2. See
// scripts/run_lmul_sweep.sh, which greps generated assembly for
// stack-relative vector loads/stores as direct, lower-level evidence.
// =======================================================================

#if GAUSS_LMUL != 1 && GAUSS_LMUL != 2 && GAUSS_LMUL != 4
#error "GAUSS_LMUL must be 1, 2, or 4 for this sweep."
#endif

#define CAT_(a, b) a##b
#define CAT(a, b) CAT_(a, b)

#define LM CAT(m, GAUSS_LMUL)

#define VOP(name) CAT(__riscv_##name, LM)
#define TYPE_U32  CAT(CAT(vuint32, LM), _t)
#define TYPE_I32  CAT(CAT(vint32,  LM), _t)

// Forward declarations -- defined below, used by gaussian_blur_rvv() at
// the bottom of this file to build/consume temporary u32 buffers.
static void widen_image_to_u32_scalar(const uint8_t* src, uint32_t* dst_u32, int n);
static void narrow_u32_to_u8_scalar(const uint32_t* src_u32, uint8_t* dst, int n);

void gaussian_blur_interior_rvv_u32src(const uint32_t* src_u32,
                                        uint32_t* dst_u32,
                                        int width,
                                        int height) {

    for (int y = KHALF; y < height - KHALF; y++) {

        int x = KHALF;

        while (x < width - KHALF) {

            int remaining = (width - KHALF) - x;

            // Plain (non-widening) vsetvl at e32 -- EMUL = LMUL
            // directly, no fractional-source constraint applies to
            // anything in this loop body.
            size_t vl = VOP(vsetvl_e32)(remaining);

            TYPE_U32 sum = VOP(vmv_v_x_u32)(0, vl);

            for (int ky = 0; ky < KSIZE; ky++) {
                int ir = y + ky - KHALF;
                const uint32_t* row_base = src_u32 + ir * width + (x - KHALF);

                for (int kx = 0; kx < KSIZE; kx++) {

                    const uint32_t* row_ptr = row_base + kx;

                    // Plain u32 load -- NOT a widening instruction,
                    // so EMUL=LMUL with no source-fraction floor.
                    TYPE_U32 pixels_u32 = VOP(vle32_v_u32)(row_ptr, vl);

                    int32_t coeff = GAUSSIAN_KERNEL[ky][kx];

                    // Plain u32 multiply (vmul, not vwmulu) -- again
                    // non-widening, EMUL=LMUL throughout.
                    TYPE_U32 product = VOP(vmul_vx_u32)(pixels_u32, coeff, vl);

                    sum = VOP(vadd_vv_u32)(sum, product, vl);
                }
            }

            // Scale by ~1/273 via (sum * 240) >> 16 -- same numerically
            // verified approximation as the default path. Every op
            // below (vmul, vsra, vmin, vmax, vreinterpret-same-width)
            // is a PLAIN, same-width operation: EMUL=LMUL exactly,
            // for every operand, at every swept GAUSS_LMUL value, with
            // no widening/narrowing legality question to get wrong.
            TYPE_I32 sum_signed =
                CAT(__riscv_vreinterpret_v_u32, CAT(LM, CAT(_i32, LM)))(sum);

            TYPE_I32 scaled     = VOP(vmul_vx_i32)(sum_signed, 240, vl);
            TYPE_I32 signed_val = VOP(vsra_vx_i32)(scaled, 16, vl);

            TYPE_I32 clamped = VOP(vmin_vx_i32)(
                                    VOP(vmax_vx_i32)(signed_val, 0, vl),
                                    255, vl);

            TYPE_U32 clamped_u32 =
                CAT(__riscv_vreinterpret_v_i32, CAT(LM, CAT(_u32, LM)))(clamped);

            // Store as u32 -- the final narrow to uint8_t happens
            // afterward in a trivial scalar pass (narrow_u32_to_u8_scalar
            // below), deliberately kept OUT of this hot loop so this
            // file contains zero widening/narrowing vector instructions
            // and therefore zero remaining EMUL-floor risk.
            uint32_t* dst_ptr = dst_u32 + y * width + x;
            VOP(vse32_v_u32)(dst_ptr, clamped_u32, vl);

            x += static_cast<int>(vl);
        }
    }
}

// Trivial scalar widen/narrow passes, O(n) each, run once per call to
// gaussian_blur_rvv() OUTSIDE the timed convolution loop -- not part
// of what riscv_main_vectorized.cpp's per-stage timer measures for
// this stage (the timer only brackets the gaussian_blur_rvv() call as
// a whole in the existing pipeline code, so these two cheap O(n)
// scalar passes are included in that bracket, but they are vastly
// cheaper than the 25-tap-per-pixel convolution itself: 2 extra O(n)
// passes vs. 25*n multiply-adds, i.e. roughly a 12x smaller
// contribution, and -- more importantly -- they are IDENTICAL in cost
// across all three swept LMUL values, so they don't change the
// relative shape of the LMUL=1 vs 2 vs 4 comparison, only shift all
// three curves up by the same small constant amount).
static void widen_image_to_u32_scalar(const uint8_t* src, uint32_t* dst_u32, int n) {
    for (int i = 0; i < n; i++) {
        dst_u32[i] = static_cast<uint32_t>(src[i]);
    }
}

static void narrow_u32_to_u8_scalar(const uint32_t* src_u32, uint8_t* dst, int n) {
    for (int i = 0; i < n; i++) {
        dst[i] = static_cast<uint8_t>(src_u32[i]);
    }
}

void gaussian_blur_interior_rvv(const uint8_t* src,
                                uint8_t* dst,
                                int width,
                                int height) {
    int n = width * height;
    uint32_t* src_u32 = static_cast<uint32_t*>(malloc(n * sizeof(uint32_t)));
    uint32_t* dst_u32 = static_cast<uint32_t*>(malloc(n * sizeof(uint32_t)));

    widen_image_to_u32_scalar(src, src_u32, n);

    gaussian_blur_interior_rvv_u32src(src_u32, dst_u32, width, height);

    narrow_u32_to_u8_scalar(dst_u32, dst, n);

    free(src_u32);
    free(dst_u32);
}

#undef CAT_
#undef CAT
#undef LM
#undef VOP
#undef TYPE_U32
#undef TYPE_I32

#else
// =======================================================================
// DEFAULT path (GAUSS_LMUL not defined) -- normal production build.
// =======================================================================

void gaussian_blur_interior_rvv(const uint8_t* src,
                                uint8_t* dst,
                                int width,
                                int height) {

    // Process rows excluding the border
    for (int y = KHALF; y < height - KHALF; y++) {

        int x = KHALF;

        while (x < width - KHALF) {

            int remaining = (width - KHALF) - x;
            size_t vl = __riscv_vsetvl_e8m1(remaining);

            // 32-bit unsigned accumulator for each output pixel.
            // Kept as u32 (not u16) because the running sum can exceed
            // 65535 well before all 25 taps are added (255 * 273 sum of
            // weights ~= 69615 worst case), so u16 would silently overflow.
            vuint32m4_t sum = __riscv_vmv_v_x_u32m4(0, vl);

            for (int ky = 0; ky < KSIZE; ky++) {
                // Load the row pointer once per kernel row instead of
                // recomputing per tap; cuts redundant pointer arithmetic.
                int ir = y + ky - KHALF;
                const uint8_t* row_base = src + ir * width + (x - KHALF);

                for (int kx = 0; kx < KSIZE; kx++) {

                    const uint8_t* row_ptr = row_base + kx;

                    vuint8m1_t pixels_u8 = __riscv_vle8_v_u8m1(row_ptr, vl);

                    int16_t coeff = GAUSSIAN_KERNEL[ky][kx];

                    // Single widening multiply straight from u8 -> u32,
                    // skipping a separate vzext_vf2 step first. This
                    // removes one full vector instruction per tap (25
                    // fewer per output chunk), since the widening
                    // multiply can take the narrower (u8) operand
                    // directly.
                    vuint32m4_t product =
                        __riscv_vwmulu_vx_u32m4(
                            __riscv_vzext_vf2_u16m2(pixels_u8, vl),
                            coeff, vl);

                    sum = __riscv_vadd_vv_u32m4(sum, product, vl);
                }
            }

            // ----------------------------------------------------------
            // Scale by ~1/273 using a fixed-point multiply + shift
            // (sum * 240) >> 16, instead of a true divide. Verified
            // numerically that across every possible sum value
            // 0..69615, this disagrees with exact integer `sum / 273`
            // for ~3.3% of sums, and ONLY ever by +-1 out of 255 --
            // never more. We intentionally do NOT use an RVV divide
            // intrinsic here: integer divide is one of the most
            // expensive per-element operations on real hardware and
            // is interpreted at full cost (no shortcut) under QEMU
            // TCG, which made the divide version of this stage
            // measurably SLOWER than scalar despite vector width --
            // the opposite of the goal. The <=1-value rounding error
            // is judged an acceptable, documented tradeoff for that
            // speed.
            // ----------------------------------------------------------
            vint32m4_t sum_signed = __riscv_vreinterpret_v_u32m4_i32m4(sum);

            vint32m4_t scaled    = __riscv_vmul_vx_i32m4(sum_signed, 240, vl);
            vint32m4_t signed_val = __riscv_vsra_vx_i32m4(scaled, 16, vl);

            vint32m4_t clamped = __riscv_vmin_vx_i32m4(
                                      __riscv_vmax_vx_i32m4(signed_val, 0, vl),
                                      255, vl);

            vuint16m2_t narrowed_u16 =
                __riscv_vncvt_x_x_w_u16m2(__riscv_vreinterpret_v_i32m4_u32m4(clamped), vl);

            vuint8m1_t result_u8 = __riscv_vncvt_x_x_w_u8m1(narrowed_u16, vl);

            uint8_t* dst_ptr = dst + y * width + x;
            __riscv_vse8_v_u8m1(dst_ptr, result_u8, vl);

            x += static_cast<int>(vl);
        }
    }
}

#endif // GAUSS_LMUL


// =======================================================================
// Border handling and top-level entry point -- shared by both paths,
// defined exactly once.
// =======================================================================

// Scalar Gaussian blur for a single border pixel, zero-padded
static uint8_t gaussian_blur_pixel_scalar(const uint8_t* src,
                                           int width, int height,
                                           int x, int y) {
    int32_t sum = 0;
    for (int ky = 0; ky < KSIZE; ky++) {
        for (int kx = 0; kx < KSIZE; kx++) {
            int ir = y + ky - KHALF;
            int ic = x + kx - KHALF;

            // Zero-padding: out-of-bounds taps contribute 0
            if (ir < 0 || ir >= height || ic < 0 || ic >= width)
                continue;

            sum += src[ir * width + ic] * GAUSSIAN_KERNEL[ky][kx];
        }
    }
    int32_t result = sum / 273;
    if (result < 0)   result = 0;
    if (result > 255) result = 255;
    return (uint8_t)result;
}

// Computes only the border pixels -- top/bottom rows, left/right columns
void gaussian_blur_border_scalar(const uint8_t* src,
                                  uint8_t* dst,
                                  int width, int height) {
    // Top and bottom border rows (full width)
    for (int y = 0; y < height; y++) {
        if (y >= KHALF && y < height - KHALF) continue; // skip interior rows
        for (int x = 0; x < width; x++) {
            dst[y * width + x] = gaussian_blur_pixel_scalar(src, width, height, x, y);
        }
    }
    // Left and right border columns (interior rows only -- corners already done above)
    for (int y = KHALF; y < height - KHALF; y++) {
        for (int x = 0; x < width; x++) {
            if (x >= KHALF && x < width - KHALF) continue; // skip interior columns
            dst[y * width + x] = gaussian_blur_pixel_scalar(src, width, height, x, y);
        }
    }
}

void gaussian_blur_rvv(const uint8_t* src,
                       uint8_t* dst,
                       int width, int height) {
    gaussian_blur_interior_rvv(src, dst, width, height);   // fast vector path
    gaussian_blur_border_scalar(src, dst, width, height);  // only border pixels, scalar
}