# Canny Edge Detection — Usage Guide

## Build

```bash
make host
```
Produces `build/canny` (x86 host binary).

---

## Running the Pipeline

### 1. Synthetic Test Images

```bash
# Generate test image
python3 scripts/generate_complex_figure.py <W> <H>

# Run pipeline
./build/canny test_<W>x<H>.raw <W> <H>

# Visualize
python3 scripts/visualize_complex_figure.py <W> <H>
```

### 2. External Real Images (from Windows)

```bash
# Copy image from Windows to WSL
cp /mnt/c/Users/Marwa/Downloads/image.png ~/canny-riscv/

# Convert to raw grayscale
python3 -c "
from PIL import Image
img = Image.open('image.png').convert('L')
w, h = img.size
with open(f'test_{w}x{h}.raw', 'wb') as f:
    f.write(img.tobytes())
print(f'Saved test_{w}x{h}.raw ({w}x{h})')
"

# Run pipeline (use W and H printed above)
./build/canny test_<W>x<H>.raw <W> <H>

# Visualize
python3 scripts/visualize_complex_figure.py <W> <H>

# Copy result back to Windows to view
cp result_<W>x<H>.png /mnt/c/Users/Marwa/Downloads/
```

---

## Output Files

| File               | Stage                             |
|--------------------|-----------------------------------|
| out_gaussian.raw   | Stage 1 — Gaussian Blur           |
| out_magnitude.raw  | Stage 2 — Gradient Magnitude      |
| out_nms.raw        | Stage 3 — Non-Maximum Suppression |
| out_threshold.raw  | Stage 4 — Double Threshold        |
| out_final.raw      | Stage 5 — Final Edges (Hysteresis)|

---

## Verified Resolutions

Tested and confirmed working on both synthetic and real images:

| Resolution | Type      | Notes                       |
|------------|-----------|-----------------------------|
| 64×64      | Synthetic | Smallest tested             |
| 100×75     | Synthetic | Non-square resolution       |
| 256×256    | Synthetic | Primary development size    |
| Any size   | External  | Portraits, animals, flowers |

---

## Double Threshold — Auto Calculation

Thresholds are computed automatically from the NMS magnitude output,
so they adapt to any image size or contrast without manual tuning:

```cpp
uint8_t max_mag = 0;
for (int i = 0; i < n; i++)
    if (nms_out[i] > max_mag) max_mag = nms_out[i];

uint8_t high_thresh = (uint8_t)(max_mag * 0.2f);
uint8_t low_thresh  = (uint8_t)(high_thresh * 0.5f);
```

| Parameter   | Formula              | Meaning                          |
|-------------|----------------------|----------------------------------|
| high_thresh | max_magnitude × 0.20 | Minimum strength for strong edge |
| low_thresh  | high_thresh × 0.50   | Minimum strength for weak edge   |

To tune — adjust the ratios in `main.cpp`:

| Use Case                | high_ratio | low_ratio |
|-------------------------|------------|-----------|
| More edges, more detail | 0.10       | 0.50      |
| Balanced (default)      | 0.20       | 0.50      |
| Fewer, cleaner edges    | 0.30       | 0.50      |

After changing ratios, rebuild and rerun:

```bash
make host
./build/canny test_<W>x<H>.raw <W> <H>
python3 scripts/visualize_complex_figure.py <W> <H>
```

---

## Synthetic Test Figure Description

The synthetic test figure (`scripts/generate_complex_figure.py`) creates a
grayscale image with four geometric primitives designed to exercise all edge
types the Canny pipeline must handle:

| Shape           | Position          | Fill | Outline | Purpose                                      |
|-----------------|-------------------|------|---------|----------------------------------------------|
| Rectangle       | Top-left quadrant | 100  | 205     | Tests straight horizontal and vertical edges |
| Ellipse         | Center            | 150  | 255     | Tests curved edges and diagonal gradients    |
| Horizontal line | Mid-height        | 255  | —       | Tests thin single-pixel horizontal edges     |
| Vertical line   | Mid-width         | 255  | —       | Tests thin single-pixel vertical edges       |

**Design choices:**
- Rectangle uses a gray fill (100) with a bright outline (205) — distinct intensity
  levels create a visible boundary without overlapping the ellipse's contrast range
- Ellipse uses a gray fill (150) with a white outline (255) — maximum contrast
  at the boundary ensures strong gradient response in Sobel
- Both shapes have different fill values (100 vs 150) so where they overlap,
  a boundary is visible and detected as an internal edge
- Lines use full white (255) against a black background for maximum edge strength

**Expected output per stage:**

| Stage            | What you should see                                         |
|------------------|-------------------------------------------------------------|
| Input            | Gray shapes with bright outlines on a black background      |
| Gaussian Blur    | Shapes with softened/blurred boundaries                     |
| Grad Magnitude   | Bright outlines around all shape boundaries                 |
| NMS              | Same outlines thinned to 1 pixel wide                       |
| Double Threshold | Strong edges (255) and weak edges (128) classified          |
| Final Edges      | Clean single-pixel edge map, weak edges connected to strong |

## Important Notes

- Always re-run the pipeline before visualizing when changing image size.
  Output `.raw` files are overwritten each run — visualizing without
  re-running will show results from the previous size, appearing black
  or corrupted.

- `.raw` files are ignored by git (see `.gitignore`).

- All visualization results are saved to the `Images/` directory locally.
  Sample results for verified resolutions (64×64, 100×75, 256×256) and
  real images (portraits, animals, flowers) are available there for reference.