#pragma once
#include <cstdint>

/*
Gradient Direction computation.

After Sobel gradient computation, we have:
  Gx → horizontal gradient
  Gy → vertical gradient

Direction describes the orientation of the edge at each pixel.
Instead of using expensive atan2(), we quantize the direction
into 4 main angles:

  0°   → horizontal edge
  45°  → diagonal (positive slope)
  90°  → vertical edge
  135° → diagonal (negative slope)

This quantization is required for Non-Maximum Suppression (NMS).

Notes:
  - dst must be pre-allocated (uint8_t array)
  - size = width * height
*/


/*
NMS only needs to know which two neighbors to compare, and the 4-direction quantization is the 
simplest way to map gradients to those neighbor pairs efficiently.
*/ 




// Compute quantized gradient direction
// Parameters:
//   src_gx → horizontal gradient (int16_t array)
//   src_gy → vertical gradient   (int16_t array)
//   dst    → output direction    (uint8_t array)
//   width  → image width
//   height → image height
void direction_compute(const int16_t* src_gx,
                       const int16_t* src_gy,
                       uint8_t* dst,
                       int width,
                       int height);