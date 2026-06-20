#include "magnitude_vectorized.h"


// =======================================================================
// PASS 1 — raw |Gx| + |Gy|, with a running max carried as a VECTOR
// =======================================================================
uint16_t sobel_magnitude_raw_rvv(const int16_t* gx,
                                  const int16_t* gy,
                                  uint16_t* raw_mag,
                                  int n) {

    // Grab the widest vector the hardware can give us for e16 elements.
    // running_max is sized to this width and lives for the whole loop --
    // it is NOT re-initialized per chunk.
    size_t vlmax = __riscv_vsetvlmax_e16m1();

    // Start the running max at zero in every lane. Using vmv_v_x here
    // (not _tu) is fine because we're filling the *entire* vlmax-wide
    // register right now, before any tail-undisturbed logic matters.
    vuint16m1_t running_max = __riscv_vmv_v_x_u16m1(0, vlmax);

    int i = 0;
    while (i < n) {

        int remaining = n - i;

        // Step 1: how many lanes can we actually process this round?
        size_t vl = __riscv_vsetvl_e16m1(remaining);

        // Step 2 & 3: load matching chunks of Gx and Gy
        vint16m1_t gx_vec = __riscv_vle16_v_i16m1(gx + i, vl);
        vint16m1_t gy_vec = __riscv_vle16_v_i16m1(gy + i, vl);

        // Step 4: |x| = max(x, -x). No dedicated abs intrinsic, so we
        // negate via reverse-subtract-from-zero, then take the
        // elementwise max against the original.
        vint16m1_t neg_gx = __riscv_vrsub_vx_i16m1(gx_vec, 0, vl);
        vint16m1_t neg_gy = __riscv_vrsub_vx_i16m1(gy_vec, 0, vl);

        vint16m1_t abs_gx = __riscv_vmax_vv_i16m1(gx_vec, neg_gx, vl);
        vint16m1_t abs_gy = __riscv_vmax_vv_i16m1(gy_vec, neg_gy, vl);

        // NOTE on correctness: this assumes |Gx|, |Gy| never actually
        // need the full int16 range (i.e. gradients don't hit
        // INT16_MIN exactly), which holds for realistic Sobel output.
        // We won't special-case that here.

        // Step 5: raw = |Gx| + |Gy|. The sum can exceed signed int16
        // range (up to ~65534), so we reinterpret as UNSIGNED before
        // adding/storing -- this is exactly why raw_mag is uint16_t.
        vuint16m1_t abs_gx_u = __riscv_vreinterpret_v_i16m1_u16m1(abs_gx);
        vuint16m1_t abs_gy_u = __riscv_vreinterpret_v_i16m1_u16m1(abs_gy);

        vuint16m1_t raw = __riscv_vadd_vv_u16m1(abs_gx_u, abs_gy_u, vl);

        // Step 6: store this chunk's raw magnitudes at the right offset
        __riscv_vse16_v_u16m1(raw_mag + i, raw, vl);

        // Step 7: fold this chunk into the running max vector.
        // We use the _tu (tail-undisturbed) form so that on the final,
        // partial-width chunk, lanes beyond vl keep whatever max value
        // they already accumulated from earlier FULL chunks, instead
        // of being clobbered or left undefined.
        running_max = __riscv_vmaxu_vv_u16m1_tu(running_max, running_max, raw, vl);

        i += static_cast<int>(vl);
    }

    // ---- Final scalar reduction: collapse the vector running_max ----
    // down to a single global max. This is the "new RVV concept":
    // a reduction intrinsic writes its scalar result into element 0
    // of a destination vector register, not as a plain scalar return.
    vuint16m1_t reduce_seed = __riscv_vmv_v_x_u16m1(0, vlmax);

    vuint16m1_t reduced =
    __riscv_vredmaxu_vs_u16m1_u16m1(
        running_max,
        reduce_seed,
        vlmax);
        
    // Extract element 0 of the reduction result -- this is the actual
    // global max across the entire image.
    uint16_t global_max = __riscv_vmv_x_s_u16m1_u16(reduced);

    return global_max;
}

// =======================================================================
// PASS 2 — normalize raw_mag[] into [0,255] using global_max
// =======================================================================
void sobel_magnitude_normalize_rvv(const uint16_t* raw_mag,
                                    uint8_t* mag,
                                    int n,
                                    uint16_t global_max) {

    // Blank-image edge case: no gradients anywhere means nothing to
    // scale against. Avoid dividing by zero -- just emit all zeros.
    if (global_max == 0) {
        for (int i = 0; i < n; i++) {
            mag[i] = 0;
        }
        return;
    }

    // Step 8: decide the scale. Every raw value should be multiplied
    // by `scale` so that the biggest one lands exactly on 255.
    //   scale = 255 / global_max
    //
    // Done in Q8 fixed-point (scale_q8 = scale * 256) so the inner
    // vectorized loop stays in pure integer math -- no floats.
    //
    // Why this can't overflow downstream: every raw_mag[i] <= global_max
    // by definition (global_max IS the max of that array), so
    //   raw_mag[i] * scale_q8 <= global_max * (255*256/global_max) = 255*256
    // which comfortably fits a 32-bit widened product.
    uint16_t scale_q8 = static_cast<uint16_t>((255u << 8) / global_max);

    int i = 0;
    while (i < n) {

        int remaining = n - i;
        size_t vl = __riscv_vsetvl_e16m1(remaining);

        // 1. load a chunk of raw_mag
        vuint16m1_t raw_chunk = __riscv_vle16_v_u16m1(raw_mag + i, vl);

        // 2. multiply every value in the chunk by the scale.
        // Widening multiply: u16 * scalar(u16) -> u32, so we don't
        // lose precision before the shift.
        vuint32m2_t scaled = __riscv_vwmulu_vx_u32m2(raw_chunk, scale_q8, vl);

        // Undo the Q8 fixed-point shift (divide by 256).
        vuint32m2_t normalized = __riscv_vsrl_vx_u32m2(scaled, 8, vl);

        // Defensive clamp to 255 -- rounding in the fixed-point
        // approximation could in principle push the max value 1 over.
        vuint32m2_t clamped = __riscv_vminu_vx_u32m2(normalized, 255, vl);

        // 3. narrow the result down to 8-bit: 32 -> 16 -> 8
        vuint16m1_t narrowed_u16 = __riscv_vncvt_x_x_w_u16m1(clamped, vl);
        vuint8mf2_t result_u8   = __riscv_vncvt_x_x_w_u8mf2(narrowed_u16, vl);

        // 4. store into mag[]
        __riscv_vse8_v_u8mf2(mag + i, result_u8, vl);

        i += static_cast<int>(vl);
    }
}

// =======================================================================
// Top-level entry point: owns the raw_mag[] scratch buffer
// =======================================================================
void sobel_magnitude_rvv(const int16_t* gx,
                          const int16_t* gy,
                          uint8_t* mag,
                          int width,
                          int height) {

    int n = width * height;

    uint16_t* raw_mag = (uint16_t*)std::malloc(n * sizeof(uint16_t));

    uint16_t global_max = sobel_magnitude_raw_rvv(gx, gy, raw_mag, n);
    sobel_magnitude_normalize_rvv(raw_mag, mag, n, global_max);

    std::free(raw_mag);
}