#!/usr/bin/env python3
"""
visualize_lmul_sweep.py

Parses the captured stdout from run_lmul_sweep.sh (each LMUL run's
printed "RVV vs SCALAR STAGE TIMING" table) and plots the Gaussian
blur RVV time against LMUL, alongside the scalar baseline for reference.

USAGE
-----
Capture the sweep output to a file once, then visualize it:

    ./run_lmul_sweep.sh > lmul_sweep_output.txt
    python3 visualize_lmul_sweep.py lmul_sweep_output.txt

The input file is expected to contain repeated blocks like:

    ================================================================
      GAUSS_LMUL = 1  (VLEN=128)
    ================================================================

    === RVV vs SCALAR STAGE TIMING (rdtime, values in Milliseconds) ===
    Stage                  |       RVV (ms) |    Scalar (ms) |    Speedup
    ...
    Gaussian blur          |       10078.903 |        1226.053 |  -722.06%
    ...

which is exactly what run_lmul_sweep.sh prints to the terminal -- so
just redirect that script's output to a file and pass it here.
"""

import re
import sys
import matplotlib.pyplot as plt


def parse_lmul_blocks(text: str):
    """
    Returns a dict: {lmul_value (int): {"rvv_ms": float, "scalar_ms": float}}
    by scanning for "GAUSS_LMUL = N" headers and the very next
    "Gaussian blur | <rvv> | <scalar> | <speedup>" line after each.
    """
    header_re = re.compile(r"GAUSS_LMUL\s*=\s*(\d+)")
    gaussian_row_re = re.compile(
        r"^Gaussian blur\s*\|\s*([\d.]+)\s*\|\s*([\d.]+)\s*\|"
    )

    results = {}
    current_lmul = None

    for line in text.splitlines():
        header_match = header_re.search(line)
        if header_match:
            current_lmul = int(header_match.group(1))
            continue

        if current_lmul is not None:
            row_match = gaussian_row_re.match(line.strip())
            if row_match:
                rvv_ms = float(row_match.group(1))
                scalar_ms = float(row_match.group(2))
                results[current_lmul] = {"rvv_ms": rvv_ms, "scalar_ms": scalar_ms}
                current_lmul = None  # only take the first Gaussian row per block

    return results


def explain_results(results: dict) -> str:
    """
    Builds a short, DATA-GROUNDED explanation from whatever values were
    actually parsed -- not a canned story. If the data doesn't show the
    expected LMUL=2-faster / LMUL=4-slower pattern, this says so plainly
    instead of forcing a narrative that isn't supported by the numbers.
    """
    lmuls = sorted(results.keys())
    lines = []

    if 1 in results and 2 in results:
        t1, t2 = results[1]["rvv_ms"], results[2]["rvv_ms"]
        if t2 < t1:
            pct = (t1 - t2) / t1 * 100
            lines.append(
                f"LMUL=1 -> LMUL=2: {t1:.0f} ms -> {t2:.0f} ms ({pct:.1f}% faster). "
                f"At LMUL=2 each vector instruction covers twice as many elements "
                f"per group as LMUL=1, so the same 25-tap kernel issues fewer "
                f"total vector instructions for the same image -- and there is "
                f"still comfortable register headroom (~16 of 32 registers used "
                f"per live group), so no spilling occurs to offset that gain."
            )
        else:
            pct = (t2 - t1) / t1 * 100
            lines.append(
                f"LMUL=1 -> LMUL=2: {t1:.0f} ms -> {t2:.0f} ms ({pct:.1f}% SLOWER, "
                f"not faster as expected). The measured data does not show the "
                f"typical LMUL=2-faster pattern here -- worth double-checking "
                f"run-to-run noise (TIMING_REPEATS) before drawing conclusions."
            )

    if 2 in results and 4 in results:
        t2, t4 = results[2]["rvv_ms"], results[4]["rvv_ms"]
        if t4 > t2:
            pct = (t4 - t2) / t2 * 100
            lines.append(
                f"LMUL=2 -> LMUL=4: {t2:.0f} ms -> {t4:.0f} ms ({pct:.1f}% slower). "
                f"At LMUL=4 each live wide value (the running sum, the freshly "
                f"loaded/widened pixels, the multiply result, plus several more "
                f"during scale/clamp/narrow) occupies 4 of the 32 architectural "
                f"vector registers per group, leaving only ~8 groups total. The "
                f"25-tap kernel keeps enough of these values simultaneously live "
                f"that the register allocator runs out of room and spills some "
                f"groups to the stack -- extra loads/stores the source code never "
                f"requested, inserted by the compiler. Those spill loads/stores "
                f"add real cycles on top of the actual convolution work, which is "
                f"why LMUL=4 is slower despite processing more elements per "
                f"instruction in principle."
            )
        else:
            lines.append(
                f"LMUL=2 -> LMUL=4: {t2:.0f} ms -> {t4:.0f} ms (LMUL=4 was NOT "
                f"slower in this run). Check the spill-count output from "
                f"run_lmul_sweep.sh for this binary -- if it's zero, the compiler "
                f"may have kept everything in registers for this particular "
                f"image size/access pattern, and the spill effect may need a "
                f"larger image or busier inner loop to manifest measurably."
            )

    if not lines:
        lines.append("Not enough LMUL values were found to compare (need at "
                      "least two of LMUL=1, 2, 4 present in the input file).")

    return "\n\n".join(lines)


def plot_results(results: dict, output_path: str = "lmul_sweep_gaussian.png"):
    if not results:
        print("ERROR: no GAUSS_LMUL blocks with a 'Gaussian blur' row were found "
              "in the input file. Check that you redirected run_lmul_sweep.sh's "
              "output correctly, and that the header/row text matches what this "
              "parser expects.", file=sys.stderr)
        sys.exit(1)

    lmuls = sorted(results.keys())
    rvv_values = [results[l]["rvv_ms"] for l in lmuls]
    scalar_values = [results[l]["scalar_ms"] for l in lmuls]

    x_labels = [f"LMUL={l}" for l in lmuls]
    x_positions = range(len(lmuls))

    fig, ax = plt.subplots(figsize=(8, 5))

    bar_width = 0.35
    ax.bar([p - bar_width / 2 for p in x_positions], rvv_values,
           width=bar_width, label="RVV (Gaussian)", color="#4C72B0")
    ax.bar([p + bar_width / 2 for p in x_positions], scalar_values,
           width=bar_width, label="Scalar (reference, same each run)", color="#888888")

    ax.set_xticks(list(x_positions))
    ax.set_xticklabels(x_labels)
    ax.set_ylabel("Time (ms)")
    ax.set_title("Gaussian Blur: RVV Time vs LMUL\n(VLEN fixed, scalar baseline shown for reference)")
    ax.legend()
    ax.grid(axis="y", linestyle="--", alpha=0.4)

    for i, v in enumerate(rvv_values):
        ax.text(i - bar_width / 2, v, f"{v:.0f}", ha="center", va="bottom", fontsize=9)
    for i, v in enumerate(scalar_values):
        ax.text(i + bar_width / 2, v, f"{v:.0f}", ha="center", va="bottom", fontsize=9)

    fig.tight_layout()
    fig.savefig(output_path, dpi=150)
    print(f"Saved: {output_path}")

    print("\nParsed values:")
    for l in lmuls:
        print(f"  LMUL={l}: RVV={results[l]['rvv_ms']:.3f} ms, "
              f"Scalar={results[l]['scalar_ms']:.3f} ms")

    print("\n--- Explanation (generated from the parsed data above) ---\n")
    print(explain_results(results))


if __name__ == "__main__":
    if len(sys.argv) != 2:
        print(f"Usage: python3 {sys.argv[0]} <captured_sweep_output.txt>", file=sys.stderr)
        sys.exit(1)

    with open(sys.argv[1], "r") as f:
        text = f.read()

    results = parse_lmul_blocks(text)
    plot_results(results)