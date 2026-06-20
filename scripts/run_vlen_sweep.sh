#!/usr/bin/env bash
# run_vlen_sweep.sh
#
# Lives in scripts/. Builds canny_rv_vectorized ONCE via `make`, then
# runs the resulting binary three times directly with qemu-riscv64
# (NOT via `make run-vectorized`) for VLEN=128/256/512. This avoids a
# real Makefile issue: the `embed-header` target has no target file,
# so make always considers it out of date and reruns it every
# invocation, which touches include/embedded_image.h and forces a full
# recompile on every `make ... run-vectorized` call -- regardless of
# VLEN. Calling qemu-riscv64 directly on the already-built binary
# sidesteps that entirely: one real build, three fast runs.
#
# Hex dump blocks (=== DUMP_START ... === DUMP_END ===) are filtered
# out live -- only the printf'd stage timing table and other non-dump
# output reaches the terminal. Nothing is written to a log file.
#
# Run from ANYWHERE -- this script finds the project root itself
# (the directory containing the Makefile, one level up from scripts/)
# and changes into it before running `make`, since `make` must be
# invoked from the directory containing the Makefile.
#
# Usage (from inside scripts/, or from the project root, or anywhere):
#   ./scripts/run_vlen_sweep.sh

set -euo pipefail

# Resolve the real path of this script, then go one directory up
# (scripts/ -> project root), regardless of the caller's current
# working directory.
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "$PROJECT_ROOT"

VLENS=(128 256 512)
BINARY="build/canny_rv_vectorized"

echo "================================================================"
echo "  BUILD: canny_rv_vectorized (once)"
echo "================================================================"
make canny_rv_vectorized

if [[ ! -f "$BINARY" ]]; then
    echo "ERROR: $BINARY not found after build." >&2
    exit 1
fi

for vlen in "${VLENS[@]}"; do
    echo ""
    echo "================================================================"
    echo "  VLEN = ${vlen}"
    echo "================================================================"
    echo ""

    qemu-riscv64 -cpu "rv64,v=true,vlen=${vlen}" "$BINARY" 2>&1 \
        | sed '/=== DUMP_START/,/=== DUMP_END ===/d'
done

echo ""
echo "================================================================"
echo "  All VLEN runs complete. (Single build, three direct QEMU runs.)"
echo "================================================================"