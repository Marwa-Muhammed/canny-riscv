#pragma once
#include <cstdint>

/*
The Sobel operator is an edge detection technique used to identify regions of rapid intensity change in an image.
It approximates the first derivative (gradient) of the image.
It detects edges by measuring how quickly pixel intensities change in both horizontal and vertical directions.
It combines differentiation and smoothing, making it less sensitive to noise than a simple derivative filter.
The Sobel operator produces two gradient components:
Gx: horizontal intensity change.
Gy: vertical intensity change.
If Gx > 0, the intensity increases from left to right
If Gx < 0, the intensity decreases from left to right
If Gx = 0, the intensity remains constant in the X directrion
Same for Y direction but from buttom to top
*/


// Sobel kernel for X direction to detects the vertical edges
/*
It works by subtracting left pixels from right pixels to detect the intensity changes 
in the horizontal direction
*/



// Sobel kernel for Y direction to detects the horizontal edges
/*
It works by subtracting buttom pixels from top pixels to detect the intensity changes 
in the vertical direction direction
*/

// First step is identifing the Sobel kernel matrices one for X and one for Y
static const int16_t SOBEL_X[3][3] = 
{
    {-1,  0,  1},
    {-2,  0,  2},
    {-1,  0,  1}
};

static const int16_t SOBEL_Y[3][3] =
{
    {-1, -2, -1},
    { 0,  0,  0},
    { 1,  2,  1}
};


// Sobel kernel size is always 3x3
static const int SOBEL_SIZE = 3;


// Next step is to estimate the gradient in X and Y directions

//  gx and gy must be pre-allocated by caller(array)
//  size = width * height * sizeof(int16_t)
//  uses zero-padding at boundaries
//  output range: -1020 to +1020  (-2x255) to (2x255)


// Generic Sobel gradient computation with zero-padding.
// PixelType  -> image pixel type      (uint8_t)
// AccumType  -> accumulator type      (int32_t)
// KernelType -> kernel coefficient type (int8_t)

void sobel_gradient(const uint8_t* src,
                    int16_t* Gx,
                    int16_t* Gy,
                    int width,
                    int height);