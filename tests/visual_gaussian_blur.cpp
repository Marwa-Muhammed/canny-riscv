// =============================================================================
// test_gaussian_blur.cpp
//
// A STANDALONE TEST PROGRAM that applies Gaussian blur to our test images
// and saves the results so we can visually compare input vs output.
//
// WHAT THIS PROGRAM DOES:
//   1. Loads each test image from disk into memory
//   2. Allocates a second buffer for the blurred output
//   3. Calls gaussian_blur() to fill the output buffer
//   4. Saves the blurred result to a new .raw file
//   5. You then run visualize_blur.py to see before/after comparison
//
// HOW TO COMPILE (on Ubuntu host, NOT cross-compiled):
//   g++ -I include src/test_gaussian_blur.cpp src/image_io.cpp
//       src/gaussian.cpp -o test_gaussian_blur
//
// HOW TO RUN:
//   ./test_gaussian_blur
//
// HOW TO VIEW RESULTS:
//   python3 visualize_blur.py
// =============================================================================


// -----------------------------------------------------------------------------
// INCLUDES — bringing in the tools we need
// -----------------------------------------------------------------------------

// gaussian.h gives us:
//   - gaussian_blur() function declaration
//   - GAUSSIAN_KERNEL (the 5x5 number grid)
//   - GAUSSIAN_DIVISOR (273)
//   - convolve() template declaration
#include "gaussian.h"

// image_io.h gives us:
//   - load_image() — reads a .raw file into memory
//   - save_image() — writes memory to a .raw file
//   - uint8_t type for pixel values
#include "image_io.h"

// cstdlib gives us:
//   - aligned_alloc() — allocates aligned memory
//   - free()          — releases memory when we're done
#include <cstdlib>

// cstdio gives us:
//   - printf() — prints messages to the terminal
#include <cstdio>


// =============================================================================
// HELPER FUNCTION: test_one_image
//
// Applies Gaussian blur to ONE test image and saves the result.
// We made this a separate function (instead of putting everything in main)
// so we can call it multiple times — once for each test image.
//
// Parameters:
//   input_file  - path to the source .raw file  (e.g. "test_rect.raw")
//   output_file - path to save blurred .raw file (e.g. "blurred_rect.raw")
//   width       - image width in pixels
//   height      - image height in pixels
//
// Returns:
//   true  if everything worked correctly
//   false if something failed (file not found, memory error, etc.)
//
// What is bool?
//   bool is a data type that holds only TWO possible values:
//     true  = yes / success / 1
//     false = no  / failure / 0
//   We use it as a return type here so the caller (main) knows
//   if this function succeeded or failed.
// =============================================================================
bool test_one_image(const char* input_file, const char* output_file,
                    int width, int height) {

    // -------------------------------------------------------------------------
    // STEP 1: Load the input image from disk into memory
    //
    // load_image() opens the .raw file, reads all its bytes into a newly
    // allocated block of aligned memory, and returns the address of that block.
    //
    // After this call:
    //   input_image points to a block of (width * height) bytes in memory
    //   input_image[0]           = pixel at row 0, column 0
    //   input_image[1]           = pixel at row 0, column 1
    //   input_image[row*width+col] = pixel at any position (row, col)
    //
    // If the file doesn't exist or memory allocation fails, load_image
    // returns nullptr — we check for this immediately after.
    // -------------------------------------------------------------------------
    uint8_t* input_image = load_image(input_file, width, height);

    // Check if loading succeeded
    // nullptr means "pointer to nothing" — load_image returns this on failure
    // Trying to use a nullptr pointer causes an immediate program crash
    if (input_image == nullptr) {
        printf("ERROR: Could not load '%s'\n", input_file);
        return false;  // tell main() that this test failed
    }

    printf("Loaded: %s (%dx%d)\n", input_file, width, height);


    // -------------------------------------------------------------------------
    // STEP 2: Allocate a SEPARATE buffer for the blurred output
    //
    // WHY do we need a separate buffer?
    // We cannot blur the image "in place" (reading and writing to the same
    // buffer). Here is why:
    //
    //   When computing the blurred value of pixel (row=5, col=10), we need
    //   the ORIGINAL values of its 25 neighbors. If we had already written
    //   blurred values into some of those neighbors' positions, we'd be
    //   reading already-modified values — giving wrong results.
    //
    //   Example of what goes wrong without separate buffers:
    //     - We blur pixel (0,0) and write the result back to position (0,0)
    //     - Now we try to blur pixel (0,1) — it needs pixel (0,0) as a neighbor
    //     - But pixel (0,0) now contains the BLURRED value, not the original!
    //     - The blur of pixel (0,1) is now computed from wrong data
    //
    //   With separate src and dst buffers:
    //     - src always contains the ORIGINAL values
    //     - dst receives the NEW blurred values
    //     - src is never modified during the whole operation
    //
    // Analogy: Imagine grading 25 students' exams by averaging each student's
    // score with their neighbors'. If you change student A's score first, then
    // use A's NEW score to compute B's score, your results are corrupted.
    // You need to read ALL original scores (src) first, THEN write all new
    // scores (dst).
    //
    // aligned_alloc(64, total_size):
    //   64    = alignment: memory starts at an address that is a multiple of 64
    //           (required for RVV vector instructions we'll use later)
    //   total = width * height bytes (one byte per pixel)
    // -------------------------------------------------------------------------
    int total_pixels = width * height;
    uint8_t* output_image = (uint8_t*)aligned_alloc(64, total_pixels);

    // Check if allocation succeeded
    if (output_image == nullptr) {
        printf("ERROR: Could not allocate output buffer for '%s'\n",
               output_file);
        free(input_image);  // MUST free input before returning
                            // (we already allocated it — can't abandon it)
        return false;
    }


    // -------------------------------------------------------------------------
    // STEP 3: Apply Gaussian blur
    //
    // This is the main operation — we call your teammate's gaussian_blur().
    //
    // gaussian_blur(src, dst, width, height) does:
    //   - Slides the 5x5 Gaussian kernel over every pixel in src
    //   - For each pixel, computes the weighted average of its 25 neighbors
    //   - Writes the smoothed result into dst
    //   - Uses zero-padding at boundaries (out-of-bounds = 0)
    //
    // Under the hood, gaussian_blur calls:
    //   convolve<uint8_t, int32_t, int16_t>(
    //       src, dst, width, height,
    //       &GAUSSIAN_KERNEL[0][0], 5, GAUSSIAN_DIVISOR)
    //
    // After this call:
    //   input_image  = UNCHANGED (still has original sharp pixels)
    //   output_image = FILLED with blurred pixel values
    // -------------------------------------------------------------------------
    printf("Applying Gaussian blur to '%s'...\n", input_file);
    gaussian_blur(input_image, output_image, width, height);
    printf("Blur complete.\n");


    // -------------------------------------------------------------------------
    // STEP 4: Save the blurred output to a .raw file
    //
    // save_image() writes all (width * height) bytes from output_image
    // to a file at the path specified by output_file.
    //
    // After this call, a new .raw file exists on disk containing the
    // blurred pixel values. You can then view it with Python.
    // -------------------------------------------------------------------------
    save_image(output_file, output_image, width, height);
    printf("Saved blurred result: %s\n\n", output_file);


    // -------------------------------------------------------------------------
    // STEP 5: Free both memory buffers
    //
    // We used aligned_alloc to allocate both buffers.
    // We MUST free both when we are done with them.
    //
    // Memory lifecycle:
    //   input_image:  allocated by load_image → used in gaussian_blur → FREE
    //   output_image: allocated here           → used in gaussian_blur
    //                                            → saved to file → FREE
    //
    // Order matters: save_image must complete BEFORE we free output_image.
    // If we freed output_image first, save_image would read freed memory
    // (called "use after free" — a serious bug that causes random crashes).
    //
    // After free(), these pointers are no longer valid.
    // We must not use input_image or output_image after these lines.
    // -------------------------------------------------------------------------
    free(input_image);
    free(output_image);

    return true;  // tell main() that everything succeeded
}


// =============================================================================
// main() — THE ENTRY POINT OF THIS PROGRAM
//
// When you run ./test_gaussian_blur in the terminal, execution starts HERE.
// main() calls test_one_image() for each of our 5 test patterns.
//
// Return value:
//   0 = program finished successfully (standard C++ convention)
//   1 = something went wrong
// =============================================================================
int main() {

    // Image dimensions — must match what generate_test_image used
    // All our test images are 64x64 pixels
    const int WIDTH  = 64;
    const int HEIGHT = 64;

    printf("=== Gaussian Blur Test ===\n\n");

    // -------------------------------------------------------------------------
    // Test each pattern we generated earlier.
    //
    // For each call to test_one_image():
    //   Parameter 1: input file  (generated by generate_test_image)
    //   Parameter 2: output file (will be created by this program)
    //   Parameter 3: width
    //   Parameter 4: height
    //
    // The blurred output files are named "blurred_*.raw" so they don't
    // overwrite the original test images.
    //
    // We store the return value in 'ok' (a bool).
    // If test_one_image() returns false, something failed — we print a warning.
    // -------------------------------------------------------------------------

    bool ok;

    // Test 1: Rectangle
    // Expected: sharp corners and edges become soft/rounded
    ok = test_one_image("test_rect.raw", "blurred_rect.raw",
                        WIDTH, HEIGHT);
    if (!ok) printf("WARNING: Rectangle test failed!\n\n");

    // Test 2: Horizontal edge
    // Expected: sharp line between white and black becomes a gradual gradient
    ok = test_one_image("test_horizontal.raw", "blurred_horizontal.raw",
                        WIDTH, HEIGHT);
    if (!ok) printf("WARNING: Horizontal edge test failed!\n\n");

    // Test 3: Vertical edge
    // Expected: sharp vertical line becomes a gradual gradient
    ok = test_one_image("test_vertical.raw", "blurred_vertical.raw",
                        WIDTH, HEIGHT);
    if (!ok) printf("WARNING: Vertical edge test failed!\n\n");

    // Test 4: Diagonal edge
    // Expected: sharp diagonal line becomes a soft diagonal gradient
    ok = test_one_image("test_diagonal.raw", "blurred_diagonal.raw",
                        WIDTH, HEIGHT);
    if (!ok) printf("WARNING: Diagonal edge test failed!\n\n");

    // Test 5: Circle
    // Expected: sharp circular edge becomes a soft glowing circle
    ok = test_one_image("test_circle.raw", "blurred_circle.raw",
                        WIDTH, HEIGHT);
    if (!ok) printf("WARNING: Circle test failed!\n\n");

    printf("=== All tests complete ===\n");
    printf("Run 'python3 visualize_blur.py' to see before/after results.\n");

    return 0;
}