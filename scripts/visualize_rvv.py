
#!/usr/bin/env python3
"""
Reads the .raw files produced by riscv_main_vectorized.cpp and creates
a single figure showing all stages of the RVV pipeline.

Input files expected in build/decoded/:

    input.raw
    gaussian.raw
    magnitude.raw
    nms.raw
    threshold.raw
    final.raw

The combined image is saved in:

    images_rvv/riscv_rvv_result_<WIDTH>x<HEIGHT>.png
"""

from pathlib import Path
import numpy as np
import matplotlib.pyplot as plt

# ------------------------------------------------------------
WIDTH = 136
HEIGHT = 136
DTYPE = np.uint8

STAGES = [
    ("input",     "Input Image"),
    ("gaussian",  "Gaussian Blur (RVV)"),
    ("magnitude", "L1 Magnitude (RVV)"),
    ("nms",       "Non-Maximum Suppression"),
    ("threshold", "Double Threshold"),
    ("final",     "Final Edges"),
]

INPUT_DIR = Path("build/decoded_rvv")
OUTPUT_DIR = Path("images_rvv")
OUTPUT_DIR.mkdir(parents=True, exist_ok=True)
# ------------------------------------------------------------


def load_raw(raw_file: Path) -> np.ndarray:
    if not raw_file.exists():
        raise FileNotFoundError(
            f"Expected file not found: {raw_file}"
        )

    img = np.fromfile(raw_file, dtype=DTYPE)

    if img.size != WIDTH * HEIGHT:
        raise ValueError(
            f"{raw_file.name}: expected {WIDTH * HEIGHT} pixels, got {img.size}"
        )

    return img.reshape((HEIGHT, WIDTH))


def main():

    images = [
        (title, load_raw(INPUT_DIR / f"{stem}.raw"))
        for stem, title in STAGES
    ]

    rows, cols = 2, 3
    fig, axes = plt.subplots(rows, cols, figsize=(4 * cols, 4 * rows))
    axes = axes.flatten()

    fig.suptitle(
        "RVV Canny Pipeline (Gaussian + L1 Magnitude)",
        fontsize=14
    )

    for ax, (title, img) in zip(axes, images):
        ax.imshow(img, cmap="gray")
        ax.set_title(title)
        ax.axis("off")

    for ax in axes[len(images):]:
        ax.axis("off")

    plt.tight_layout()

    output_path = (
        OUTPUT_DIR /
        f"riscv_rvv_result_{WIDTH}x{HEIGHT}.png"
    )

    plt.savefig(output_path, dpi=300)
    plt.show()

    print(f"Figure saved to {output_path}")
if __name__ == "__main__":
    main()


