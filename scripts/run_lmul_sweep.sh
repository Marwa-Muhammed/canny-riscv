#!/usr/bin/env bash
# run_lmul_sweep.sh
#
# Lives in scripts/. Sweeps GAUSS_LMUL = 1, 2, 4 for the Gaussian RVV
# kernel. LMUL is compile-time, so this builds THREE separate binaries
# (cannot be done with one build + a runtime flag the way VLEN sweeps
# can).
#
# src/gaussian_vectorized.cpp contains BOTH implementations in one
# file, selected by the GAUSS_LMUL macro:
#   - not defined  -> normal production path (used by every other
#                     build in this project, unaffected by this script)
#   - defined(1/2/4) -> the LMUL register-pressure sweep path, with
#                     every live vector value held at the same swept
#                     LMUL throughout, which is what lets LMUL=4
#                     compile and run at all (the production path's
#                     widening chain cannot reach LMUL=4).
#
# At each LMUL value, also emits assembly (-S) for just this file and
# greps it for vector loads/stores that address the stack (sp) or
# frame pointer (s0) -- direct, lower-level evidence of register
# spilling, not just an inference from slower timing. Expect near-zero
# spill-pattern matches at LMUL=1/2 and a nonzero count at LMUL=4.
#
# Run from ANYWHERE -- this script finds the project root itself
# (the directory containing the Makefile/src/include, one level up
# from scripts/) and resolves every source path against that root.
#
# Usage:
#   ./scripts/run_lmul_sweep.sh > lmul_sweep_output.txt
#   ./scripts/run_lmul_sweep.sh | tee lmul_sweep_output.txt

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "$PROJECT_ROOT"

LMULS=(1 2 4)
VLEN=128   # fixed for this sweep -- only LMUL varies here

RV_CXX="riscv64-unknown-elf-g++"
RV_FLAGS="-std=c++17 -Wall -Wextra -march=rv64gcv -mabi=lp64d -O2 -static"

SRC_DIR="${PROJECT_ROOT}/src"
INCLUDE_DIR="${PROJECT_ROOT}/include"
BUILD_DIR="${PROJECT_ROOT}/build/lmul_sweep"
GAUSS_SRC="${SRC_DIR}/gaussian_vectorized.cpp"

if [[ ! -f "$GAUSS_SRC" ]]; then
    echo "ERROR: $GAUSS_SRC not found." >&2
    exit 1
fi

# All sources EXCEPT main.cpp/riscv_main.cpp/Generate_test_image.cpp.
# gaussian_vectorized.cpp is deliberately included in this list (not
# excluded) -- it's compiled once per LMUL value below with the
# matching -DGAUSS_LMUL flag, same file, three different macro values.
OTHER_SRCS=$(find "$SRC_DIR" -name "*.cpp" \
    ! -name "main.cpp" \
    ! -name "riscv_main.cpp" \
    ! -name "Generate_test_image.cpp")

mkdir -p "$BUILD_DIR"

for lmul in "${LMULS[@]}"; do
    echo ""
    echo "================================================================"
    echo "  GAUSS_LMUL = ${lmul}  (VLEN=${VLEN})"
    echo "================================================================"
    echo ""

    bin="${BUILD_DIR}/canny_rv_lmul${lmul}"
    asm="${BUILD_DIR}/gaussian_lmul${lmul}.s"

    "$RV_CXX" $RV_FLAGS -DGAUSS_LMUL=${lmul} -I "$INCLUDE_DIR" \
        $OTHER_SRCS \
        -o "$bin"

    # Separate compile of just gaussian_vectorized.cpp to assembly,
    # with the same -DGAUSS_LMUL, so we can check for register spills
    # directly in the generated code for this specific LMUL value.
    "$RV_CXX" $RV_FLAGS -DGAUSS_LMUL=${lmul} -I "$INCLUDE_DIR" \
        -S "$GAUSS_SRC" -o "$asm"

    spill_count=$(grep -cE "v[ls][0-9]?e(8|16|32)\.v.*,\s*[0-9-]*\((sp|s0)\)" "$asm" || true)

    echo "--- Spill check (vector loads/stores addressing sp/s0 in ${asm}) ---"
    echo "    Count: ${spill_count}"
    echo ""

    qemu-riscv64 -cpu "rv64,v=true,vlen=${VLEN}" "$bin" 2>&1 \
        | sed '/=== DUMP_START/,/=== DUMP_END ===/d'
done

echo ""
echo "================================================================"
echo "  LMUL sweep complete. Assembly files in ${BUILD_DIR}/ for manual"
echo "  inspection if you want to see the actual spill instructions."
echo "================================================================"