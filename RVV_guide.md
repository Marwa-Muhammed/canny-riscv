# Phase 6 – RISC-V Vector Extension (RVV) Implementation Guide

## Overview

Phase 6 extends the Canny edge detector by introducing **RISC-V Vector Extension (RVV)** acceleration. The Gaussian blur and Sobel magnitude stages are vectorized using RVV intrinsics, while the remaining stages reuse the scalar implementation.

The phase also introduces:

* QEMU timing support using host wall-clock time.
* Visualization scripts for RVV output.
* VLEN scaling experiments.
* LMUL sensitivity experiments.
* Unit tests for the vectorized kernels.
* Makefile support for RVV builds.

---

# Directory Structure

```
include/
    gaussian_vectorized.h
    magnitude_vectorized.h

src/
    gaussian_vectorized.cpp
    magnitude_vectorized.cpp
    qemu_clock.cpp
    riscv_main.cpp

tests/
    test_gaussian_vectorized.cpp
    test_magnitude_vectorized.cpp

scripts/
    run_vlen_sweep.sh
    run_lmul_sweep.sh
    visualize_rvv.py
    visualize_lmul_sweep.py

build/
    decoded_rvv/
    lmul_sweep/

images_rvv/
```

---

# Added Source Files

---

## gaussian_vectorized.h / gaussian_vectorized.cpp

Implements RVV acceleration for the Gaussian blur stage.

The file contains two versions of the vectorized interior kernel:

- **Default production path** (used in normal builds), which uses the standard widening chain and optimized fixed-point scaling.
- **LMUL sweep path** (enabled with `GAUSS_LMUL=1,2,4`), used exclusively for studying register pressure and vector register spilling during LMUL experiments.

Border handling and the top-level interface are shared between both implementations.

### Components

#### 1. gaussian_blur_interior_rvv()

Computes only interior pixels whose complete 5×5 neighborhood lies inside the image.

- Uses RVV strip-mining.
- Processes multiple pixels simultaneously.
- Selects one of two vectorized implementations at compile time:
  - default production kernel;
  - LMUL sweep kernel.

---

#### 2. gaussian_blur_border_scalar()

Processes border pixels using scalar code.

Uses zero-padding to preserve the same behavior as the scalar reference implementation.

---

#### 3. gaussian_blur_rvv()

Top-level Gaussian function.

Combines:

```
Interior region → RVV
Borders         → Scalar
```

so that every pixel is computed exactly once.

---

### Implementation Strategy

```
Entire image
      |
      |
-----------------------
|                     |
Interior          Borders
(RVV)             (Scalar)
|                     |
-----------------------
        |
     Output image
```

The vectorized section uses:

- RVV intrinsics
- Strip mining
- Dynamic vector length (`vsetvl`)
- Compile-time selection between the production kernel and the LMUL sweep kernel

---
# magnitude_vectorized.h / magnitude_vectorized.cpp

Implements RVV acceleration for Sobel magnitude computation.

Uses L1 norm:

```
Magnitude = |Gx| + |Gy|
```

The implementation is split into two passes.

---

## Pass 1 : Raw Magnitude

Implemented by:

```cpp
sobel_magnitude_raw_rvv()
```

For every pixel:

```
|Gx|
|Gy|

raw = |Gx| + |Gy|
```

Stores:

```
raw_mag[]
```

while maintaining a vector-valued running maximum.

At the end:

```
vredmaxu
```

reduces all lanes to obtain:

```
global_max
```

---

## Pass 2 : Normalization

Implemented by:

```cpp
sobel_magnitude_normalize_rvv()
```

Computes:

```
255/global_max
```

using fixed-point arithmetic.

Converts:

```
raw_mag[]
```

into the final 8-bit magnitude image.

---

## Top-Level Function

```cpp
sobel_magnitude_rvv()
```

Performs:

```
Pass 1
↓
global_max
↓
Pass 2
↓
Magnitude image
```

---

# qemu_clock.cpp

Provides a Linux syscall-based implementation of:

```cpp
clock_gettime()
```

for the

```
riscv64-unknown-elf-g++
+
qemu-riscv64
```

environment.

---

## Motivation

Newlib on the bare-metal toolchain does not provide a usable implementation of:

```cpp
clock_gettime()
```

and QEMU's cycle counters are not cycle-accurate.

Using:

```cpp
rdcycle
```

would not provide meaningful execution times.

---

## Solution

Issue Linux syscalls using:

```cpp
ecall
```

which are intercepted by QEMU user mode and forwarded to the host kernel.

Thus:

```
RISC-V program
      ↓
ecall
      ↓
QEMU
      ↓
Host Linux
      ↓
CLOCK_MONOTONIC
```

providing real wall-clock timing and enabling fair host vs RVV comparisons.

---

# riscv_main.cpp

Main RVV application.

Runs the Canny pipeline:

```
Input
 ↓
Gaussian Blur (RVV)
 ↓
Sobel X/Y
 ↓
Magnitude (RVV)
 ↓
Non-Maximum Suppression
 ↓
Double Threshold
 ↓
Hysteresis
 ↓
Final Edges
```

Also:

* Measures execution time for each stage.
* Dumps intermediate images.
* Generates .raw outputs inside:

```
build/decoded_rvv/
```

---

# Unit Tests

## test_gaussian_vectorized.cpp

Verifies correctness of:

```cpp
gaussian_blur_rvv()
```

Checks RVV output against the scalar Gaussian implementation.

Ensures:

* Identical numerical results.
* Correct border handling.

---

## test_magnitude_vectorized.cpp

Verifies:

```cpp
sobel_magnitude_rvv()
```

against the scalar Sobel magnitude stage.

Checks:

* Absolute values.
* Normalization.
* Corner cases.
* Global maximum reduction.

---

# VLEN Sweep

Script:

```
scripts/run_vlen_sweep.sh
```

Evaluates performance under different vector lengths.

### Tested VLEN values

```
128
256
512
```

The binary is built once:

```
make canny_rv_vectorized
```

and executed multiple times:

```bash
qemu-riscv64 -cpu rv64,v=true,vlen=<VLEN>
```

This avoids repeated recompilation.

---

## Running

```bash
./scripts/run_vlen_sweep.sh
```

---

# LMUL Sweep

Script:

```
scripts/run_lmul_sweep.sh
```

Evaluates register pressure effects.

Gaussian kernel is compiled with:

```
GAUSS_LMUL = 1
GAUSS_LMUL = 2
GAUSS_LMUL = 4
```

using:

```cpp
-DGAUSS_LMUL=<value>
```

---

## Register Spill Analysis

The script emits assembly:

```
gaussian_lmul1.s
gaussian_lmul2.s
gaussian_lmul4.s
```

and searches for vector spills involving:

```
sp
s0
```

using grep.

The output reports spill counts and timing measurements.

---

## Running

```bash
./scripts/run_lmul_sweep.sh > lmul_sweep_output.txt
```

or

```bash
./scripts/run_lmul_sweep.sh | tee lmul_sweep_output.txt
```

---

# RVV Pipeline Visualization

Script:

```
visualize_rvv.py
```

Reads:

```
build/decoded_rvv/

input.raw
gaussian.raw
magnitude.raw
nms.raw
threshold.raw
final.raw
```

and generates a combined figure.

Saved into:

```
images_rvv/
```

as:

```
riscv_rvv_result_<WIDTH>x<HEIGHT>.png
```

---

## Running

```bash
python3 scripts/visualize_rvv.py
```

---

# LMUL Visualization

Script:

```
visualize_lmul_sweep.py
```

Reads:

```
lmul_sweep_output.txt
```

and produces plots illustrating:

* Execution time.
* LMUL scaling behavior.
* Register spilling trends.

---

# Makefile Updates

The Makefile was extended to support:

### RVV compilation

Using:

```bash
-march=rv64gcv
-mabi=lp64d
```

and building:

```bash
make canny_rv_vectorized
```

---

### RVV tests

```bash
make test_gaussian_vectorized

make test_magnitude_vectorized
```

---

### Automatic inclusion

The new files:

```
gaussian_vectorized.cpp
magnitude_vectorized.cpp
qemu_clock.cpp
riscv_main.cpp
```

are included automatically in the RVV build.

---

# Running Phase 6

## Build

```bash
make canny_rv_vectorized
```

---

## Execute

```bash
qemu-riscv64 \
-cpu rv64,v=true,vlen=128 \
build/canny_rv_vectorized
```

---

## Run Unit Tests

```bash
make test_gaussian_vectorized

make test_magnitude_vectorized
```

---

## VLEN Sweep

```bash
./scripts/run_vlen_sweep.sh
```

---

## LMUL Sweep

```bash
./scripts/run_lmul_sweep.sh > lmul_sweep_output.txt
```

---

## Visualize RVV Pipeline

```bash
python3 scripts/visualize_rvv.py
```

---

## Visualize LMUL Results

```bash
python3 scripts/visualize_lmul_sweep.py
```

---

# Summary

Phase 6 introduces RVV acceleration to the Canny edge detector by vectorizing:

* Gaussian blur.
* Sobel magnitude.

while preserving the scalar implementations for the remaining stages.

Additional infrastructure provides:

* Accurate timing under QEMU.
* Unit testing.
* VLEN performance scaling.
* LMUL register-pressure analysis.
* Visualization utilities.
* Extended Makefile support.

Together these components enable both correctness validation and performance exploration of RVV-based image processing.
