#!/bin/bash

# Generate test images
./build/generate_test

# Run Canny on each image
IMAGES=("test_rect" "test_horizontal" "test_vertical" "test_diagonal" "test_circle")

for IMG in "${IMAGES[@]}"; do
    echo "Processing $IMG..."
    ./build/canny_host ${IMG}.raw 64 64
    
    # Rename outputs for each image
    mv out_gaussian.raw   ${IMG}_gaussian.raw
    mv out_magnitude.raw  ${IMG}_magnitude.raw
    mv out_nms.raw        ${IMG}_nms.raw
    mv out_threshold.raw  ${IMG}_threshold.raw
    mv out_final.raw      ${IMG}_final.raw
done

echo "Done! Run visualize.py to view results."
