# RISC-V Canny Benchmark — Phase 4 & Phase 5 Documentation

## Project Context

This benchmark runs a 5-stage Canny edge detection pipeline (`riscv_main.cpp`) on both
a RISC-V target (via QEMU) and a native x86 host build, then compares
their performance. Phase 4 and Phase 5 cover, respectively, **how the comparison sweep is
automated** and **how the underlying timing measurements are made trustworthy**.

| Phase | Theme | Key files |
|---|---|---|
| 4 | Compiler Optimization Sweep | `opt_sweep.sh`, `parse_timing.py` |
| 5 | Real Wall-Clock Timing on RISC-V | `qemu_clock.cpp`, timing system in `riscv_main.cpp` |

---

## Phase 4 — Compiler Optimization Sweep

### Purpose

`opt_sweep.sh` automates building and running the Canny pipeline across **six optimization
levels** (`-O0, -O1, -O2, -O3, -Os, -Ofast`) for **two targets** (RISC-V cross-compiled
binary under QEMU, and a native host build), then produces a side-by-side comparison of
binary size, vectorization behavior, and per-stage timing.

### Usage

```bash
./opt_sweep.sh [scalar|vector]
```

| Mode | `-march` | QEMU flags | Effect |
|---|---|---|---|
| `scalar` (default) | `rv64gc` | `-cpu rv64,v=false,vlen=128` | No vector extension; baseline scalar codegen |
| `vector` | `rv64gcv` | `-cpu rv64,v=true,vlen=128` | Enables RVV (vector) extension, VLEN=128 |

Common RISC-V build flags applied at every optimization level:
```
-std=c++17 -Wall -Wextra -march=<rv64gc|rv64gcv> -mabi=lp64d -static
```

### What the script does, step by step

1. **Setup** — resolves the project root, creates `logs/`, and initializes three log
   files for this run: a size CSV, a `vset` instruction-count CSV, and a combined
   text log.
2. **Host source discovery** — globs `src/*.cpp`, excluding `main.cpp`,
   `Generate_test_image.cpp`, and `qemu_clock.cpp` (the latter is RISC-V/QEMU-only and
   would not compile/link meaningfully on the host).
3. **RISC-V sweep** (for each of `O0…Ofast`):
   - `make clean` then `make canny_rv_embedded RV_FLAGS="<base flags> -<OPT>"`.
   - Records the resulting binary size (`stat -c%s`) to the size CSV.
   - Counts `vset*` instructions in the disassembly (`objdump -d | grep -c vset`) as a
     proxy for vector-instruction usage — only meaningful in `vector` mode.
   - Runs the binary under `qemu-riscv64` with the mode-appropriate `-cpu` flags,
     redirecting stdout/stderr to `logs/qemu_<OPT>.log`. Failures are logged and the
     sweep continues with the next optimization level.
4. **Host sweep** (for each of `O0…Ofast`):
   - Native `g++` build of the same source set, output to
     `build/canny_host_<OPT>`, run directly, output captured to
     `logs/host_<OPT>.log`.
5. **Vectorization report** (`vector` mode only):
   - Rebuilds at `-O3` with `-ftree-vectorize -fopt-info-vec-optimized`, capturing
     GCC's vectorization diagnostics to `logs/opt_sweep_vecinfo_O3_<vector>.log`.
   - Counts and lists every `"loop vectorized"` diagnostic line, printing file + line
     number for each.
6. **Results tables**:
   - Binary size table and `vset` count table, printed via `column -s, -t`.
   - **Per-opt-level RISC-V vs. host timing**, delegated to `parse_timing.py` in
     `--combined` mode (or `--table` mode if only the RISC-V log is available).

### Output artifacts (all under `logs/`)

| File | Contents |
|---|---|
| `opt_sweep_sizes_<mode>.csv` | `opt_level,binary_size_bytes` |
| `opt_sweep_vset_counts_<mode>.csv` | `opt_level,vset_instruction_count` |
| `opt_sweep_vecinfo_O3_<mode>.log` | Raw GCC `-fopt-info-vec-optimized` output (vector mode only) |
| `opt_sweep_combined_<mode>.txt` | All per-stage timing tables for every optimization level |
| `qemu_<OPT>.log`, `host_<OPT>.log` | Raw stdout of each individual run, consumed by `parse_timing.py` |

### `parse_timing.py` — log parsing and table rendering

This script never runs anything itself; it only parses the **stdout produced by the
benchmark binary** (RISC-V and/or host) and renders it as readable tables or CSV.

---

## Phase 5 — Real Wall-Clock Timing via QEMU Syscall Passthrough

### The problem this phase solves

To compare RISC-V and host performance meaningfully, both sides need a timer that
measures the **same notion of time** (real wall-clock nanoseconds), not just an
RISC-V-cycle counter that QEMU doesn't even model cycle-accurately. Two obstacles stood
in the way of the bare-metal RISC-V target:

1. `clock_gettime()` is not implemented by the bare-metal newlib environment used for
   `riscv64-unknown-elf-g++` cross-compilation, so it's undeclared/unusable out of the box.
2. Even if it were declared, rdcycle/rdtime-based cycle countsonly give *relative* timing under QEMU,
   since QEMU's user-mode emulator doesn't model RISC-V cycles 1:1 with real elapsed time.

### The fix: `qemu_clock.cpp`

This file supplies a **custom implementation of `clock_gettime()`** that bypasses
newlib entirely.


### The timing system in `riscv_main.cpp`

**`BENCH(label, call)` macro** — the core measurement primitive:
1. **Warmup** — runs `call` 5 times, untimed, to prime caches/branch predictors and
   absorb any lazy first-call initialization inside the stage.
2. **Timed loop** — runs `call` `ITERATIONS` (100) times, bracketing each iteration with
   `read_time()` before/after and a memory-fence `asm volatile` in between to prevent
   the compiler from dead-code-eliminating or merging the call across iterations.
3. Prints the average as ` <label> : <avg_ns> <timing_name> (avg over 100 runs)` —
   this exact line format is what `parse_timing.py`'s `STAGE_RE` regex expects.

**Pipeline structure (`run_pipeline`)** — benchmarks all five Canny stages in order for
a given magnitude mode:

```
src → Gaussian Blur → Sobel Gradient (Gx,Gy) → Magnitude (L1 or L2) → Direction
    → Non-Maximum Suppression → Double Threshold → Hysteresis → final_out
```
**`main()` flow:**
1. Loads the compile-time-embedded image (`embedded_image.h`), sanity-checking that
   `width * height` matches the embedded byte-array length.
2. Copies the embedded (read-only/rodata) image into a freshly aligned, writable buffer
   so pipeline stages never alias flash/rodata memory.
3. Runs `run_pipeline` twice — once with `use_l2 = false` (L1/Manhattan magnitude,
   cheaper, no sqrt) and once with `use_l2 = true` (L2/Euclidean magnitude, more
   accurate) — each wrapped in its own outer `read_time()` pair to get total wall time
   for all 100 timed iterations plus warmup.
4. Prints a final summary: total time and average single-pass time (`total / 100`) for
   both L1 and L2 — these are exactly the `L1/L2 total time` / `L1/L2 single pass` lines
   `parse_timing.py`'s `PIPE_TOT_MS_RE` / `PIPE_SINGLE_RE` patterns capture.
