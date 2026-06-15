// =============================================================================
// generate_test_image.cpp
//
// A STANDALONE PROGRAM that generates synthetic test images with known patterns.
//
// WHY THIS FILE EXISTS:
//   To test our Canny edge detection pipeline, we need images with PREDICTABLE
//   content. We know EXACTLY where the edges should appear.
//
// WHAT THIS PROGRAM CREATES:
//   1. test_rect.raw       - white rectangle on black background
//   2. test_horizontal.raw - top half white, bottom half black (horizontal edge)
//   3. test_vertical.raw   - left half black, right half white (vertical edge)
//   4. test_diagonal.raw   - diagonal dividing line
//
// HOW TO COMPILE (on your Ubuntu host, NOT cross-compiled):
//   g++ -I include src/generate_test_image.cpp src/image_io.cpp -o generate_test
//
// HOW TO RUN:
//   ./generate_test
//
// HOW TO VIEW the generated images (using Python):
//   python3 visualize.py
// =============================================================================


// Our image_io.h gives us:
//   - load_image() and save_image() function declarations
//   - uint8_t type (8-bit unsigned integer, perfect for pixel values 0-255)
#include "image_io.h"

// cstdlib gives us:
//   - aligned_alloc() : allocates memory at a guaranteed aligned address
//   - free()          : releases memory when we are done with it
#include <cstdlib>

// cstring gives us:
//   - memset() : fills an entire block of memory with one value very quickly
//               instead of writing a loop to set each byte individually
#include <cstring>

// cstdio gives us:
//   - printf() : prints text to the terminal so we know what's happening
#include <cstdio>


// =============================================================================
// HELPER FUNCTION: allocate_black_image
//
// Creates a new image buffer filled entirely with black (value 0).
// Every pattern starts with a completely black canvas, then we "paint"
// specific pixels white to create the pattern we want.
//
// Parameters:
//   width  - number of columns in pixels
//   height - number of rows in pixels
//
// Returns:
//   uint8_t* - pointer to the allocated and zero-filled buffer
//              caller must free() this when done
// =============================================================================
uint8_t* allocate_black_image(int width, int height) {

    // Calculate total number of pixels (= total bytes needed)
    int total_pixels = width * height;

    // Allocate aligned memory
    uint8_t* image = (uint8_t*)aligned_alloc(64, total_pixels);

    // check if allocation succeeded 
   
    if (image == nullptr) {
        printf("ERROR: allocate_black_image: memory allocation failed\n");
        return nullptr;
    }

    // Fill the ENTIRE image with 0 (black)
    // memset(destination, value, number_of_bytes)
    // This is much faster than a for loop because memset is
    // optimized at a low level to fill memory as fast as possible
    //
    // After this call:
    //   image[0]   = 0 (black)
    //   image[1]   = 0 (black)
    //   image[2]   = 0 (black)
    //   ... every single pixel is 0
    memset(image, 0, total_pixels);

    return image;
}


// =============================================================================
// PATTERN 1: generate_rectangle
//
// Creates a white rectangle in the CENTER of a black image.
//   A rectangle has 4 edges: top, bottom, left, right.
//   After edge detection, we should see EXACTLY those 4 lines appear.
//   This tests that our detector finds edges in all four directions.
// =============================================================================
void generate_rectangle(int width, int height) {

    // Step 1: Start with a completely black image
    uint8_t* image = allocate_black_image(width, height);
    if (image == nullptr) return;  // stop if allocation failed

    // Step 2: Define the rectangle boundaries
    // We place the rectangle in the CENTER quarter of the image
    // Using width/4 and height/4 instead of hardcoded numbers means
    // this works correctly for ANY image size, not just 64x64
    int rect_top    = height / 4;
    int rect_bottom = height * 3 / 4;
    int rect_left   = width  / 4;
    int rect_right  = width  * 3 / 4;

    // Step 3: Paint the rectangle white using nested loops
    //
    // OUTER LOOP: goes through each ROW from rect_top to rect_bottom
    // INNER LOOP: for each row, goes through each COLUMN from rect_left to rect_right
    //
    // For every pixel INSIDE the rectangle boundaries → set to 255 (white)
    // Pixels OUTSIDE the loops stay 0 (black) from the memset above
    //
    // The formula (row * width + col) converts 2D position to 1D array index:
    //   row * width  = skip past all the complete rows above this one
    //   + col        = then move right to the correct column
    //
    // Example: pixel at row=2, col=3 in a width=8 image:
    //   index = 2 * 8 + 3 = 16 + 3 = 19
    //   meaning: the 19th byte in our flat array
    for (int row = rect_top; row < rect_bottom; row++) {
        for (int col = rect_left; col < rect_right; col++) {
            image[row * width + col] = 255;  // white pixel
        }
    }

    // Step 4: Save the image to a .raw file using our save_image function
    // After this call, a file called "test_rect.raw" appears on your disk
    save_image("test_rect.raw", image, width, height);
    printf("Generated: test_rect.raw (%dx%d)\n", width, height);

    // Step 5: Free the memory
    free(image);
}


// =============================================================================
// PATTERN 2: generate_horizontal_edge
//
// Creates an image where the TOP HALF is white and BOTTOM HALF is black.
// The boundary between them is a sharp HORIZONTAL EDGE.
//
// WHY THIS PATTERN?
//   The Sobel-Y kernel specifically detects TOP-TO-BOTTOM changes in intensity.
//   On this image, Sobel-Y should produce a large response at the middle row,
//   and Sobel-X should produce near-zero response (no left-right changes).
//   This lets us verify that Sobel-Y is working correctly in isolation.
// =============================================================================
void generate_horizontal_edge(int width, int height) {

    // Step 1: Start with a completely black image (all zeros)
    uint8_t* image = allocate_black_image(width, height);
    if (image == nullptr) return;

    // Step 2: Paint the TOP HALF white
    //
    // "Top half" means rows 0 to (height/2 - 1)
    // For a 64x64 image: rows 0 to 31
    //
    // OUTER LOOP: only goes through the TOP HALF of rows (0 to height/2)
    // INNER LOOP: goes through EVERY column (0 to width)
    //             because we want entire rows to be white, not just part
    //
    // Bottom half stays black (0) from the memset in allocate_black_image
    for (int row = 0; row < height / 2; row++) {
        for (int col = 0; col < width; col++) {
            image[row * width + col] = 255;  // white pixel
        }
    }

    // Step 3: Save and free
    save_image("test_horizontal.raw", image, width, height);
    printf("Generated: test_horizontal.raw (%dx%d)\n", width, height);
    free(image);
}


// =============================================================================
// PATTERN 3: generate_vertical_edge
//
// Creates an image where the LEFT HALF is black and RIGHT HALF is white.
// The boundary between them is a sharp VERTICAL EDGE.
//
// WHY THIS PATTERN?
//   The Sobel-X kernel specifically detects LEFT-TO-RIGHT changes in intensity.
//   On this image, Sobel-X should produce a large response at the middle column,
//   and Sobel-Y should produce near-zero response (no top-bottom changes).
//   This lets us verify that Sobel-X is working correctly in isolation.
// =============================================================================
void generate_vertical_edge(int width, int height) {

    // Step 1: Start with completely black image
    uint8_t* image = allocate_black_image(width, height);
    if (image == nullptr) return;

    // Step 2: Paint the RIGHT HALF white
    //
    // "Right half" means columns (width/2) to (width-1)
    // For a 64x64 image: columns 32 to 63
    //
    // OUTER LOOP: goes through EVERY row (top to bottom)
    // INNER LOOP: only goes through the RIGHT HALF of columns
    //
    // Notice: this is the OPPOSITE structure from generate_horizontal_edge
    //   Horizontal edge: outer loop restricted (rows), inner loop full (cols)
    //   Vertical edge:   outer loop full (rows), inner loop restricted (cols)
    for (int row = 0; row < height; row++) {
        for (int col = width / 2; col < width; col++) {
            image[row * width + col] = 255;  // white pixel
        }
    }

    // Step 3: Save and free
    save_image("test_vertical.raw", image, width, height);
    printf("Generated: test_vertical.raw (%dx%d)\n", width, height);
    free(image);
}


// =============================================================================
// PATTERN 4: generate_diagonal_edge
//
// Creates an image divided by a diagonal line from top-left to bottom-right.
// Above/left of the diagonal = black. Below/right of the diagonal = white.
//
// WHY THIS PATTERN?
//   A diagonal edge activates BOTH Sobel-X and Sobel-Y simultaneously.
//   This tests that both kernels work together correctly and that the
//   gradient direction is computed correctly (~45 degrees for this pattern).
// =============================================================================
void generate_diagonal_edge(int width, int height) {

    // Step 1: Start with completely black image
    uint8_t* image = allocate_black_image(width, height);
    if (image == nullptr) return;

    // Step 2: Paint pixels WHITE where column index > row index
    //
    // We visit EVERY pixel in the image using nested loops.
    // For each pixel at position (row, col), we make a decision:
    //
    //   if col > row  → this pixel is to the RIGHT of the diagonal → WHITE (255)
    //   if col <= row → this pixel is ON or LEFT of the diagonal   → BLACK (0)
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {

            if (col > row) {
                // This pixel is to the right of the diagonal → white
                image[row * width + col] = 255;
            }
            // else: pixel stays 0 (black) from the memset
            // we don't need an explicit else because memset already set it to 0
        }
    }

    // Step 3: Save and free
    save_image("test_diagonal.raw", image, width, height);
    printf("Generated: test_diagonal.raw (%dx%d)\n", width, height);
    free(image);
}

// =============================================================================
// PATTERN 5: generate_circle
//
// Creates a filled white circle in the CENTER of a black image.
//
// WHY THIS PATTERN?
//   A circle has edges at EVERY possible angle simultaneously:
//     - Top/bottom of circle → horizontal edges (Sobel-Y responds strongly)
//     - Left/right of circle → vertical edges   (Sobel-X responds strongly)
//     - Diagonal parts       → both kernels respond together
//   This makes a circle the most COMPLETE test of your edge detector.
//   After Canny processing, you should see a perfect ring outline appear.
//
// HOW DO WE KNOW IF A PIXEL IS INSIDE THE CIRCLE?
//   We use the circle equation. A pixel at position (col, row) is inside
//   a circle centered at (center_x, center_y) with radius r if:
//
//     (col - center_x)² + (row - center_y)²  <  radius²
//
//   This is the distance formula SQUARED - we avoid the square root
//   because square roots are slow for computers to calculate.
//   Squaring both sides gives the exact same yes/no answer but faster.
//
// WHAT IT LOOKS LIKE (tiny 9x9 example, center=4,4 radius=3):
//
//   col→  0  1  2  3   4   5  6  7  8
//   row↓
//    0  [  0  0  0   0   0   0  0  0  0 ]   far from center → black
//    1  [  0  0  0  255 255 255  0  0  0 ]   within radius  → white
//    2  [  0  0 255 255 255 255 255  0  0 ]
//    3  [  0 255 255 255 255 255 255 255  0 ]
//    4  [  0 255 255 255 255 255 255 255  0 ]  ← center row
//    5  [  0 255 255 255 255 255 255 255  0 ]
//    6  [  0  0 255 255 255 255 255  0  0 ]
//    7  [  0  0  0  255 255 255  0  0  0 ]
//    8  [  0  0  0   0   0   0  0  0  0 ]
//
// Parameters:
//   width  - image width in pixels
//   height - image height in pixels
// =============================================================================
void generate_circle(int width, int height) {

    // -------------------------------------------------------------------------
    // STEP 1: Start with a completely black image
    // -------------------------------------------------------------------------
    uint8_t* image = allocate_black_image(width, height);

    // Always check if memory allocation succeeded before using the pointer
    if (image == nullptr) return;


    // -------------------------------------------------------------------------
    // STEP 2: Calculate the circle's center point
    // -------------------------------------------------------------------------
    int center_x = width  / 2;   // horizontal center = middle column
    int center_y = height / 2;   // vertical center   = middle row


    // -------------------------------------------------------------------------
    // STEP 3: Calculate the radius
    //
    // We want the circle to fill most of the image but leave a small
    // black border around it so the edges are clearly visible.
    //
    // We use the SMALLER of width and height divided by 3.
    // The "smaller of" part handles non-square images correctly.
    //
    // Why divide by 3?
    //   If we divided by 2, the circle would touch the edges exactly.

    // The expression (width < height ? width : height) means:
    //   "if width is smaller than height, use width, otherwise use height"
    // -------------------------------------------------------------------------
    int radius = (width < height ? width : height) / 3;


    // -------------------------------------------------------------------------
    // STEP 4: Pre-calculate radius squared
    //
    // For every single pixel, we will check if it's inside the circle.
    // The check involves comparing to radius².
    //
    // Instead of calculating (radius * radius) freshly for EVERY pixel
    // (which would be thousands of multiplications), we calculate it
    // ONCE here and store it in radius_sq.
    //
    // For a 64x64 image: radius=21, radius_sq = 21*21 = 441
    //
    // This is a small optimization but a good habit: compute things
    // OUTSIDE the loop if their value doesn't change inside the loop.
    // -------------------------------------------------------------------------
    int radius_sq = radius * radius;


    // -------------------------------------------------------------------------
    // STEP 5: Visit every pixel and decide: inside circle or outside?
    //
    // We use TWO NESTED LOOPS to visit every single pixel in the image:
    //
    //   OUTER LOOP (row): moves from top to bottom of the image
    //   INNER LOOP (col): for each row, moves from left to right
    // -------------------------------------------------------------------------
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {


            // -----------------------------------------------------------------
            // STEP 6: Calculate this pixel's distance from the circle center
            // dc = "delta column" = horizontal distance from center
            // dr = "delta row" = vertical distance from center
            // -----------------------------------------------------------------
            int dc = col - center_x;   // horizontal distance from center
            int dr = row - center_y;   // vertical distance from center


            // -----------------------------------------------------------------
            // STEP 7: Calculate distance SQUARED and compare to radius SQUARED
            //
            // The true distance from center would be:
            //   distance = √(dc² + dr²)
            //
            // But square roots are slow! Instead we compare:
            //   dc² + dr²  vs  radius²
            //
            // This gives the SAME yes/no answer:
            //   distance < radius   is the same as   dc²+dr² < radius²
            // -----------------------------------------------------------------
            if (dc*dc + dr*dr < radius_sq) {

                // This pixel is INSIDE the circle → paint it WHITE
                image[row * width + col] = 255;   // white

            }
            // If NOT inside circle: pixel stays 0 (black) from memset

        }  // end inner loop (columns)
    }  // end outer loop (rows)


    // -------------------------------------------------------------------------
    // STEP 8: Save the image to a .raw file
    //
    // -------------------------------------------------------------------------
    save_image("test_circle.raw", image, width, height);
    printf("Generated: test_circle.raw (%dx%d) center=(%d,%d) radius=%d\n",
           width, height, center_x, center_y, radius);


    // -------------------------------------------------------------------------
    // STEP 9: Free the memory
    // -------------------------------------------------------------------------
    free(image);
}


// =============================================================================
// main() - THE ENTRY POINT OF THIS PROGRAM
// =============================================================================
int main() {

    // We use 64x64 for all test images.
  
    const int WIDTH  = 64;
    const int HEIGHT = 64;

    printf("Generating test images (%dx%d)...\n", WIDTH, HEIGHT);
    printf("------------------------------------\n");

    // Generate each pattern
    // Each function: allocates memory → draws pattern → saves file → frees memory
    generate_rectangle(WIDTH, HEIGHT);
    generate_horizontal_edge(WIDTH, HEIGHT);
    generate_vertical_edge(WIDTH, HEIGHT);
    generate_diagonal_edge(WIDTH, HEIGHT);
    generate_circle(WIDTH, HEIGHT); 

    printf("------------------------------------\n");
    printf("Done! Run visualize.py to view the images.\n");

    return 0; 
}