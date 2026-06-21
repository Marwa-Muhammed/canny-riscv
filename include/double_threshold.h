
#include <cstdint>

// Output pixel values used to classify edges.
// These exact values are expected by the hysteresis stage that follows.
static constexpr uint8_t STRONG_EDGE = 255; // Definite edge - magnitude >= high_thresh
static constexpr uint8_t WEAK_EDGE   = 128; // Candidate edge - low_thresh <= magnitude < high_thresh
static constexpr uint8_t NO_EDGE     = 0;   // Not an edge - magnitude < low_thresh

/**
 * double_threshold()
 *
 * Stage 4 of the Canny pipeline. Takes the normalized gradient magnitude
 * image and classifies every pixel into one of three categories based on
 * two thresholds, producing a map that the hysteresis stage will use.
 *
 * @param magnitude   Input: normalized gradient magnitude image (uint8_t, 0-255)
 * @param output      Output: classified edge map (uint8_t), same dimensions
 * @param width       Image width in pixels
 * @param height      Image height in pixels
 * @param low_thresh  Lower threshold. Pixels below this are suppressed (set to 0)
 * @param high_thresh Upper threshold. Pixels at or above this are strong edges (set to 255)
 *                    Pixels between low and high become weak edges (set to 128)
 *
 * Note: A typical threshold ratio is high = 2x or 3x low (e.g. low=50, high=150)
 */
void double_threshold(const uint8_t* magnitude,
                      uint8_t*       output,
                      int            width,
                      int            height,
                      uint8_t        low_thresh,
                      uint8_t        high_thresh);

