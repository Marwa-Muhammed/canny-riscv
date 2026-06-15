#pragma once
#include <cstdint>

/*
Gradient Magnitude computation.
After Sobel gradient computation, we have two arrays:
  Gx → horizontal gradient (left-right changes)
  Gy → vertical gradient   (top-bottom changes)

Magnitude combines them into ONE value representing
how strong the edge is at each pixel.

Two methods:
  L1 norm: |Gx| + |Gy|        → fast, integer only
  L2 norm: sqrt(Gx² + Gy²)    → accurate, uses float

Output is normalized to [0, 255] so it can be saved as image.

Notes:
  - dst must be pre-allocated by caller
  - size = width * height * sizeof(uint8_t)
  - Two passes needed: first find max, then normalize
  - L1 may overestimate diagonal edges
  - L2 is mathematically correct
*/

// Compute L1 magnitude: |Gx| + |Gy|
// Parameters:
//   src_gx → horizontal gradient (int16_t array)
//   src_gy → vertical gradient   (int16_t array)
//   dst    → output magnitude    (uint8_t array)
//   width  → image width
//   height → image height
void magnitude_l1(const int16_t* src_gx,
                  const int16_t* src_gy,
                  uint8_t* dst,
                  int width,
                  int height);

// Compute L2 magnitude: sqrt(Gx² + Gy²)
// Parameters:
//   src_gx → horizontal gradient (int16_t array)
//   src_gy → vertical gradient   (int16_t array)
//   dst    → output magnitude    (uint8_t array)
//   width  → image width
//   height → image height
void magnitude_l2(const int16_t* src_gx,
                  const int16_t* src_gy,
                  uint8_t* dst,
                  int width,
                  int height);