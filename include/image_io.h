// =============================================================================
// image_io.h
//
// This header file DECLARES two functions:
//   1. load_image  - reads a raw grayscale image file and allocate it in memory
//   2. save_image  - writes pixel data from memory to a raw file
//
// =============================================================================


// -----------------------------------------------------------------------------
// A header guard
// -----------------------------------------------------------------------------
#pragma once


// -----------------------------------------------------------------------------
// This brings in definitions for PRECISE integer types.
// -----------------------------------------------------------------------------
#include <cstdint>


// -----------------------------------------------------------------------------
// #include <cstddef>
//
// This brings in the definition for size_t.
// -----------------------------------------------------------------------------
#include <cstddef>


// -----------------------------------------------------------------------------
// FUNCTION DECLARATION: load_image
//
// PURPOSE:
//   Opens a raw grayscale image file, reads all its pixel bytes into a
//   freshly allocated block of memory, and returns the address of that memory.
//
// WHAT IS A RAW GRAYSCALE IMAGE?
//   A file that is EXACTLY (width * height) bytes long.
//   Each byte represents one pixel. No headers, no compression, nothing else.
//   Byte value 0   = black pixel
//   Byte value 255 = white pixel
//   Values in between = shades of gray
//
// PARAMETERS:
//
//   const char* filename
//     The path to the .raw file to open and read.

//   int width
//     How many columns (pixels across) the image has.
//     We must pass this because raw files have no header - the file itself
//     contains ONLY pixel values, no dimension information.
//     Example: 512 for a 512-pixel wide image.
//
//   int height
//     How many rows (pixels tall) the image has.
//     Same reason as width - raw files store no metadata.
//     Example: 512 for a 512-pixel tall image.
//
// RETURN VALUE:
//   uint8_t*  (a pointer to uint8_t)
//   You can then visit any pixel by calculating:
//     address of pixel(row, col) = first_address + (row * width + col)
//
//   Returns nullptr (address zero = "nothing") if:
//     - The file does not exist or cannot be opened
//     - Memory allocation fails
//     - The file is smaller than expected
//
// IMPORTANT - MEMORY OWNERSHIP:
//   This function allocates memory using aligned_alloc().
//   The CALLER (whoever calls load_image) is responsible for
//   freeing this memory when done, using: free(pointer)
//   Forgetting to free memory causes a "memory leak."
// -----------------------------------------------------------------------------
uint8_t* load_image(const char* filename, int width, int height);


// -----------------------------------------------------------------------------
// FUNCTION DECLARATION: save_image
//
// PURPOSE:
//   Takes pixel data that exists in memory and writes it to a raw file on disk.
//   After this function runs, you will have a .raw file you can view with Python.
//
// PARAMETERS:
//
//   const char* filename
//     The path where the output file should be created.
//     Example: "output.raw" or "results/blurred.raw"
//     If this file already exists, it will be OVERWRITTEN.
//     If it does not exist, it will be CREATED.
//
//   const uint8_t* buffer
//     A pointer to the pixel data in memory that we want to save.
//     This is the ADDRESS of the first pixel.
//     "const" means this function PROMISES not to modify the pixel values -
//     it only reads them to write to the file.
//     This is important: save_image should never accidentally change your image.
//
//   int width
//     Number of columns in the image.
//     Used to calculate total bytes to write: width * height
//
//   int height
//     Number of rows in the image.
//     Used to calculate total bytes to write: width * height
//
// RETURN VALUE:
//   void
// -----------------------------------------------------------------------------
void save_image(const char* filename, const uint8_t* buffer, int width, int height);