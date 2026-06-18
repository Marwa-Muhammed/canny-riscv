#!/usr/bin/env python3
"""
Reads the raw grayscale stages produced by riscv_main.cpp's L1/L2
magnitude comparison (in build/decoded/), plus the single shared
input.raw dump, and produces TWO combined grid figures -- one for the
L1 (|Gx| + |Gy|) pipeline run, one for the L2 (sqrt(Gx^2 + Gy^2)) run --
saved as Images/riscv_result_l1_<WIDTH>x<HEIGHT>.png and
Images/riscv_result_l2_<WIDTH>x<HEIGHT>.png.

Adjust WIDTH, HEIGHT, and DTYPE according to your image format.
"""

from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

# -------------------------------------------------------------------
WIDTH = 683
HEIGHT = 691
DTYPE = np.uint8

# Per-technique stages dumped inside run_pipeline()'s DUMP(...) calls.
STAGE_SUFFIXES = [
    ("gaussian",  "Gaussian Blur"),
    ("magnitude", "Gradient Magnitude"),
    ("nms",       "NMS"),
    ("threshold", "Double Threshold"),
    ("final",     "Final Edges"),
]

INPUT_DIR  = Path("build/decoded")
OUTPUT_DIR = Path("Images")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
# -------------------------------------------------------------------


def load_raw(raw_file: Path) -> np.ndarray:
    if not raw_file.exists():
        raise FileNotFoundError(
            f"Expected stage file not found: {raw_file}\n"
            f"Did make run-embedded-decode actually run, and did it "
            f"produce both the l1_ and l2_ stages?"
        )
    img = np.fromfile(raw_file, dtype=DTYPE)
    if img.size != WIDTH * HEIGHT:
        raise ValueError(
            f"{raw_file.name}: expected {WIDTH * HEIGHT} pixels, got {img.size}"
        )
    return img.reshape((HEIGHT, WIDTH))


def load_input() -> np.ndarray:
    # Single shared dump -- not prefixed by l1_/l2_, same src for both passes.
    return load_raw(INPUT_DIR / "input.raw")


def load_stage(prefix: str, suffix: str) -> np.ndarray:
    return load_raw(INPUT_DIR / f"{prefix}_{suffix}.raw")


def plot_technique(prefix: str, label: str, input_img: np.ndarray):
    """Builds and saves one combined figure for a single technique (l1 or l2)."""
    images = [("Input Image", input_img)] + [
        (title, load_stage(prefix, stem)) for stem, title in STAGE_SUFFIXES
    ]

    rows, cols = 2, 3
    fig, axes = plt.subplots(rows, cols, figsize=(4 * cols, 4 * rows))
    axes = axes.flatten()
    fig.suptitle(f"{label} magnitude pipeline", fontsize=14)

    for ax, (title, img) in zip(axes, images):
        ax.imshow(img, cmap="gray")
        ax.set_title(title)
        ax.axis("off")

    # 6 images into a 2x3 grid -- exact fit, no hidden axes needed.
    for ax in axes[len(images):]:
        ax.axis("off")

    plt.tight_layout()
    combined_path = OUTPUT_DIR / f"riscv_result_{prefix}_{WIDTH}x{HEIGHT}.png"
    plt.savefig(combined_path, dpi=300)
    plt.show()
    print(f"{label} figure saved to {combined_path}")


input_img = load_input()
plot_technique("l1", "L1  |Gx| + |Gy|", input_img)
plot_technique("l2", "L2  sqrt(Gx^2 + Gy^2)", input_img)