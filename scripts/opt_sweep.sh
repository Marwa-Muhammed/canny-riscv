#!/usr/bin/env bash
#
# Phase 4: Compiler Optimization Sweep
# Updated for simplified rdtime-based timing

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "${SCRIPT_DIR}/.." && pwd)"
cd "$PROJECT_ROOT"

LOG_DIR="${PROJECT_ROOT}/logs"
mkdir -p "$LOG_DIR"

BUILD_DIR=build
OBJDUMP=riscv64-unknown-elf-objdump
QEMU_BIN=qemu-riscv64
VLEN=128

MODE="${1:-scalar}"

if [[ "$MODE" == "scalar" ]]; then
    MARCH="rv64gc"
    QEMU_FLAGS="-cpu rv64,v=false,vlen=${VLEN}"
    SUFFIX="scalar"
elif [[ "$MODE" == "vector" ]]; then
    MARCH="rv64gcv"
    QEMU_FLAGS="-cpu rv64,v=true,vlen=${VLEN}"
    SUFFIX="vector"
else
    echo "usage: $0 [scalar|vector]" >&2
    exit 1
fi

echo "========================================================"
echo " Mode: $MODE  (-march=${MARCH})"
echo " Timing: clock_gettime (RISC-V wall-clock, ns)"
echo "========================================================"

BASE_RV_FLAGS="-std=c++17 -Wall -Wextra -march=${MARCH} -mabi=lp64d -static"

SIZE_LOG="${LOG_DIR}/opt_sweep_sizes_${SUFFIX}.csv"
VSET_LOG="${LOG_DIR}/opt_sweep_vset_counts_${SUFFIX}.csv"
VECINFO_LOG="${LOG_DIR}/opt_sweep_vecinfo_O3_${SUFFIX}.log"
COMBINED_LOG="${LOG_DIR}/opt_sweep_combined_${SUFFIX}.txt"

echo "opt_level,binary_size_bytes"      > "$SIZE_LOG"
echo "opt_level,vset_instruction_count" > "$VSET_LOG"
> "$COMBINED_LOG"

# Host sources
HOST_SRCS=$(find src -name "*.cpp" \
    | grep -v "src/main.cpp" \
    | grep -v "src/Generate_test_image.cpp" \
    | grep -v "src/qemu_clock.cpp" \
    | tr '\n' ' ')
declare -A RV_LOGS
declare -A HOST_LOGS

# ── RISC-V Sweep ─────────────────────────────────────────────────
echo
echo "╔══════════════════════════════════════════╗"
echo "║        RISC-V Optimization Sweep         ║"
echo "╚══════════════════════════════════════════╝"

for OPT in O0 O1 O2 O3 Os Ofast; do
    echo
    echo "=== [RISC-V $OPT] building ==="
    make clean >/dev/null 2>&1
    make canny_rv_embedded RV_FLAGS="${BASE_RV_FLAGS} -${OPT}" >/dev/null 2>&1

    BIN="${BUILD_DIR}/canny_rv_embedded"
    SIZE=$(stat -c%s "$BIN")
    echo "${OPT},${SIZE}" >> "$SIZE_LOG"

    VSET_COUNT=$("$OBJDUMP" -d "$BIN" | grep -c "vset" || true)
    echo "${OPT},${VSET_COUNT}" >> "$VSET_LOG"

    RUNLOG="${LOG_DIR}/qemu_${OPT}.log"
    RV_LOGS[$OPT]="$RUNLOG"
    echo "  running under QEMU..."
    if ! "$QEMU_BIN" $QEMU_FLAGS "$BIN" > "$RUNLOG" 2>&1; then
        echo "  !! QEMU run FAILED -- see ${RUNLOG}"
        continue
    fi
    echo "  RISC-V ${OPT} OK"
done

# ── Host Sweep ───────────────────────────────────────────────────
echo
echo "╔══════════════════════════════════════════╗"
echo "║        Host (x86) Optimization Sweep     ║"
echo "╚══════════════════════════════════════════╝"

for OPT in O0 O1 O2 O3 Os Ofast; do
    echo
    echo "=== [host $OPT] building ==="
    g++ -std=c++17 -Wall -Wextra -${OPT} -I include \
        $HOST_SRCS -o "${BUILD_DIR}/canny_host_${OPT}"

    HOST_RUNLOG="${LOG_DIR}/host_${OPT}.log"
    HOST_LOGS[$OPT]="$HOST_RUNLOG"
    echo "  running host build..."
    if "${BUILD_DIR}/canny_host_${OPT}" > "$HOST_RUNLOG" 2>&1; then
        echo "  host ${OPT} OK"
    else
        echo "  !! host ${OPT} FAILED"
    fi
done

# ── Vectorization Report ─────────────────────────────────────────
if [[ "$MODE" == "vector" ]]; then

echo
echo "╔══════════════════════════════════════════╗"
echo "║     Auto-Vectorization Report (-O3)      ║"
echo "╚══════════════════════════════════════════╝"
make clean >/dev/null
make canny_rv_embedded \
    RV_FLAGS="${BASE_RV_FLAGS} -O3 -ftree-vectorize -fopt-info-vec-optimized" \
    2> "$VECINFO_LOG" >/dev/null || true

LOOPS_VEC=$(grep -c "loop vectorized" "$VECINFO_LOG" 2>/dev/null || echo 0)
printf " %-30s %10s\n" "Loops vectorized:" "$LOOPS_VEC"
echo
echo "  Vectorized loops:"
grep "loop vectorized" "$VECINFO_LOG" | while IFS= read -r line; do
    FILE=$(echo "$line" | cut -d: -f1 | xargs basename)
    LINENO=$(echo "$line" | cut -d: -f2)
    printf "    %-30s line %s\n" "$FILE" "$LINENO"
done
echo
echo "  Full report: $VECINFO_LOG"
fi

# ── Results Tables ───────────────────────────────────────────────
echo
echo "╔══════════════════════════════════════════╗"
echo "║    Binary Sizes (RISC-V)                 ║"
echo "╚══════════════════════════════════════════╝"
column -s, -t "$SIZE_LOG"

echo
echo "╔══════════════════════════════════════════╗"
echo "║    vset Instruction Counts               ║"
echo "╚══════════════════════════════════════════╝"
column -s, -t "$VSET_LOG"

# ── Combined Results ──────────────────────────────────────────────
echo
echo "╔══════════════════════════════════════════╗"
echo "║  RISC-V vs Host: Per-Stage Timings       ║"
echo "╚══════════════════════════════════════════╝"

for OPT in O0 O1 O2 O3 Os Ofast; do
    echo
    echo "════════════════════════════════════════════"
    echo "  Optimization level: -${OPT}"
    echo "════════════════════════════════════════════"

    RV_LOG="${RV_LOGS[$OPT]:-}"
    HOST_LOG="${HOST_LOGS[$OPT]:-}"

    if [[ -n "$RV_LOG" && -f "$RV_LOG" && -n "$HOST_LOG" && -f "$HOST_LOG" ]]; then
        python3 "${SCRIPT_DIR}/parse_timing.py" "$OPT" "$RV_LOG" "$HOST_LOG" --combined
    elif [[ -n "$RV_LOG" && -f "$RV_LOG" ]]; then
        echo "  (host result unavailable)"
        python3 "${SCRIPT_DIR}/parse_timing.py" "$OPT" "$RV_LOG" --table
    fi

    {
        echo ""
        echo "=== ${OPT} ==="
        echo ""
        if [[ -n "$RV_LOG" && -f "$RV_LOG" && -n "$HOST_LOG" && -f "$HOST_LOG" ]]; then
            python3 "${SCRIPT_DIR}/parse_timing.py" "$OPT" "$RV_LOG" "$HOST_LOG" --combined
        elif [[ -n "$RV_LOG" && -f "$RV_LOG" ]]; then
            python3 "${SCRIPT_DIR}/parse_timing.py" "$OPT" "$RV_LOG" --table
        fi
    } >> "$COMBINED_LOG" 2>&1
done

echo
echo "Done."
echo "  Size CSV     : ${SIZE_LOG}"
echo "  vset CSV     : ${VSET_LOG}"
echo "  Combined log : ${COMBINED_LOG}"
echo "  Vec report   : ${VECINFO_LOG:-N/A (scalar mode)}"
echo "  Logs dir     : ${LOG_DIR}/"
