

#include "image_io.h"

// Gives us: aligned_alloc() and free()
// aligned_alloc - allocates a block of memory at an aligned address
// free          - releases memory that was allocated with aligned_alloc
#include <cstdlib>

// Gives us: fopen(), fread(), fwrite(), fclose(), fprintf()
// These are the standard C file input/output functions
// fopen  - opens a file and returns a FILE* handle
// fread  - reads bytes FROM a file INTO memory
// fwrite - writes bytes FROM memory INTO a file
// fclose - closes an open file (MUST always be done when finished)
// fprintf - prints formatted text to a stream (we use it for error messages)
#include <cstdio>


// =============================================================================
// FUNCTION: load_image
//
// Reads a raw grayscale image file from disk into a block of memory.
// Returns the address (pointer) of that memory block.
// =============================================================================
uint8_t* load_image(const char* filename, int width, int height) {

    // -------------------------------------------------------------------------
    // STEP 1: Calculate how many bytes we need
    //
    // A raw grayscale image is EXACTLY (width * height) bytes.
    // Each pixel = 1 byte. No headers, no extra data.
    // -------------------------------------------------------------------------
    int total_pixels = width * height;


    // -------------------------------------------------------------------------
    // STEP 2: Allocate aligned memory to hold the pixel data
    //
    //  aligned_alloc(alignment, size) :
    //  Give me a block of memory that is:
    //  exactly 'size' bytes large (total_pixels bytes)
    //  starting at a memory address that is a multiple of 'alignment' (64)"
    //
    // -------------------------------------------------------------------------
    uint8_t* buffer = (uint8_t*)aligned_alloc(64, total_pixels);


    // -------------------------------------------------------------------------
    // STEP 3: Check that memory allocation succeeded
    // -------------------------------------------------------------------------
    if (buffer == nullptr) {
        fprintf(stderr, "ERROR: load_image: could not allocate %d bytes\n",
                total_pixels);
        return nullptr;  // tell the caller "I failed, here is nothing"
    }


    // -------------------------------------------------------------------------
    // STEP 4: Open the file for reading
    //
    // fopen(filename, mode) asks the operating system to open a file.
    // It returns a FILE* - a "file handle" 
    //
    // Mode "rb" means:
    //   r = open for READING 
    //   b = binary mode (raw bytes, not text)
    // If the file does not exist, fopen returns nullptr.
    // -------------------------------------------------------------------------
    FILE* file = fopen(filename, "rb");


    // -------------------------------------------------------------------------
    // STEP 5: Check that the file opened successfully
    // -------------------------------------------------------------------------
    if (file == nullptr) {
        fprintf(stderr, "ERROR: load_image: could not open file '%s'\n",
                filename);
        free(buffer);   // MUST free memory before returning, to avoid leak
        return nullptr;
    }


    // -------------------------------------------------------------------------
    // STEP 6: Read all pixel bytes from the file into our buffer
    //
    // fread(destination, element_size, element_count, file_handle)
    //
    //   destination   = buffer   (WHERE in memory to put the data)
    //   element_size  = 1        (each element is 1 byte = 1 pixel)
    //   element_count = total_pixels (how many elements to read)
    //   file_handle   = file     (which open file to read from)
    //
    // fread returns the NUMBER OF ELEMENTS actually read.
    // In most successful cases this equals total_pixels.
    // If it returns less, the file was shorter than expected (a problem).
    //
    // What happens physically:
    // The operating system reads bytes one by one from the file on disk
    // and copies them into consecutive memory locations starting at buffer:
    //   buffer[0] = first byte from file  = pixel at row 0, column 0
    //   buffer[1] = second byte from file = pixel at row 0, column 1
    //   buffer[2] = third byte from file  = pixel at row 0, column 2
    //   ... and so on for all total_pixels bytes
    // -------------------------------------------------------------------------
    int pixels_read = fread(buffer, 1, total_pixels, file);


    // -------------------------------------------------------------------------
    // STEP 7: Verify we read the correct number of bytes
    //
    // If pixels_read != total_pixels, something went wrong:
    // - The file might be smaller than width*height bytes
    // - The file might be corrupted
    // - There might have been a read error
    // -------------------------------------------------------------------------
    if (pixels_read != total_pixels) {
        fprintf(stderr,
                "ERROR: load_image: expected %d bytes but read %d from '%s'\n",
                total_pixels, pixels_read, filename);
        fclose(file);   // close the file first
        free(buffer);   // then free the memory
        return nullptr;
    }


    // -------------------------------------------------------------------------
    // STEP 8: Close the file
    // -------------------------------------------------------------------------
    fclose(file);


    // -------------------------------------------------------------------------
    // STEP 9: Return the address of our filled buffer
    // -------------------------------------------------------------------------
    return buffer;
}


// =============================================================================
// FUNCTION: save_image
//
// Writes pixel data from memory to a raw grayscale file on disk.
// Returns nothing (void) - the result is a file on your disk.
// =============================================================================
void save_image(const char* filename, const uint8_t* buffer,
                int width, int height) {

    // -------------------------------------------------------------------------
    // STEP 1: Calculate total bytes to write
    // Total file size = width * height bytes (one byte per pixel).
    // -------------------------------------------------------------------------
    int total_pixels = width * height;


    // -------------------------------------------------------------------------
    // STEP 2: Open the file for writing
    //
    // Mode "wb" means:
    //   w = open for WRITING
    //   b = binary mode (raw bytes, not text)
    //
    // Behavior:
    //   - If the file does NOT exist: it gets CREATED as a new empty file
    //   - If the file DOES exist: it gets OVERWRITTEN from the beginning
    //
    // This is fine for our use case - we always want the latest output.
    // -------------------------------------------------------------------------
    FILE* file = fopen(filename, "wb");


    // -------------------------------------------------------------------------
    // STEP 3: Check that the file opened/created successfully
    // -------------------------------------------------------------------------
    if (file == nullptr) {
        fprintf(stderr, "ERROR: save_image: could not create file '%s'\n",
                filename);
        return;  // nothing more we can do - just return early
    }


    // -------------------------------------------------------------------------
    // STEP 4: Write all pixel bytes from buffer to the file
    //
    // fwrite(source, element_size, element_count, file_handle)
    //
    //   source        = buffer       (WHERE in memory to read the data FROM)
    //   element_size  = 1            (each element is 1 byte = 1 pixel)
    //   element_count = total_pixels (how many elements to write)
    //   file_handle   = file         (which open file to write to)
    //
    // fwrite returns the number of elements actually written.
    // This should equal total_pixels if everything worked correctly.
    //
    // What happens physically:
    // The operating system reads bytes from your memory buffer
    // and writes them to the file on disk:
    //   file byte 0 = buffer[0] = pixel at row 0, column 0
    //   file byte 1 = buffer[1] = pixel at row 0, column 1
    //   ... and so on
    //
    // The result is a file that is EXACTLY total_pixels bytes long.
    // -------------------------------------------------------------------------
    int pixels_written = fwrite(buffer, 1, total_pixels, file);


    // -------------------------------------------------------------------------
    // STEP 5: Check that we wrote everything successfully
    // -------------------------------------------------------------------------
    if (pixels_written != total_pixels) {
        fprintf(stderr,
                "ERROR: save_image: wrote %d of %d bytes to '%s'\n",
                pixels_written, total_pixels, filename);
    }


    // -------------------------------------------------------------------------
    // STEP 6: Close the file
    // -------------------------------------------------------------------------
    fclose(file);
    
    // The caller will find their file on disk at the path they specified.
}