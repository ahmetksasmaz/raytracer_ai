#pragma once

// Image signal processing: what a camera does to a RAW to make a picture.
//
// Everything here operates on plain float buffers and knows nothing about the
// scene or the renderer. The stages are in pipeline order, and each one is also
// its own executable:
//
//   black level    DN               -> [0,1] linear, against the fixed window
//   demosaic       mosaic           -> three full-resolution channels
//   white balance  sensorRGB        -> neutral-corrected sensorRGB
//   colour matrix  sensorRGB        -> CIE XYZ
//   sRGB encode    CIE XYZ          -> 8-bit sRGB
//
// Only the last stage leaves a linear, physically meaningful space. Everything
// before it is data to compute on; the PNG at the end is for looking at.

#include <string>
#include <vector>

#include "Core/Types.hpp"
#include "SensorModel.hpp"

namespace isp {

// --- Black level ------------------------------------------------------------
// Maps digital numbers onto [0,1] through the sensor's fixed display window:
// saturation at the top of the ADC range, black a declared number of stops
// below. Both are constants of the sensor, never of the frame -- a camera does
// not re-expose itself to suit the scene.
//
// This runs BEFORE the demosaic, which is where a real ISP subtracts black
// level and is a change from the pre-split pipeline, where the mapping happened
// after. The two do not commute at clipped pixels: clamping first means a
// blown neighbour contributes its clipped value to the interpolation rather
// than its true one, which is what the hardware does.
//
// `clipped_high` and `clipped_low` count pixels at the ends of the window.
// Those counts are the measurement, not a warning -- a sensor that cannot fit
// the scene's range blows highlights and crushes shadows, and that is a result.
void ApplyBlackLevel(const std::vector<FP_PRECISION>& dn,
                     const SensorModel& sensor,
                     std::vector<FP_PRECISION>* normalised,
                     size_t* clipped_high = nullptr,
                     size_t* clipped_low = nullptr);

// --- Demosaic ---------------------------------------------------------------
// Takes the pixel's own value where the CFA carries that colour, and otherwise
// averages the neighbours in the 3x3 window that do.
//
// Deliberately simple. This exists so the mosaic can be looked at and processed,
// not as a demosaic study; a better interpolation is a drop-in replacement for
// this one function.
void Demosaic(int width, int height, const std::vector<FP_PRECISION>& mosaic,
              const SensorModel& sensor, std::vector<FP_PRECISION> rgb[3]);

// --- White balance ----------------------------------------------------------
enum class WhiteBalanceMethod {
  kGains,      // use the gains as supplied
  kGrayWorld,  // assume the average of the scene is neutral
  kMaxRGB,     // assume the brightest reading in each channel is neutral
};

bool WhiteBalanceMethodByName(const std::string& name,
                              WhiteBalanceMethod* method);

// Estimates gains from the image itself. Both estimators are guesses about the
// scene rather than measurements of the sensor, which is why the calibrated
// gains from `sensor_ccm` are the default elsewhere: those come from the
// illuminant and the sensor's own curves, and are right by construction.
void EstimateWhiteBalance(const std::vector<FP_PRECISION> rgb[3],
                          WhiteBalanceMethod method, FP_PRECISION gains[3]);

// Applies per-channel gains in place. Green is conventionally 1.0.
void ApplyWhiteBalance(std::vector<FP_PRECISION> rgb[3],
                       const FP_PRECISION gains[3]);

// --- Colour matrix ----------------------------------------------------------
// sensorRGB -> CIE XYZ. The matrix must match whether white balance was applied
// first; see Sensor/SensorCalibration.hpp.
void ApplyColorMatrix(const std::vector<FP_PRECISION> rgb[3],
                      const FP_PRECISION m[3][3],
                      std::vector<FP_PRECISION> xyz[3]);

// --- Display encode ---------------------------------------------------------
// The sRGB transfer function -- the real piecewise one, not a plain 2.2 gamma.
FP_PRECISION EncodeSRGB(FP_PRECISION v);
unsigned char Quantise8(FP_PRECISION unit);

// CIE XYZ -> 8-bit sRGB, interleaved RGB. The clamp is the sRGB gamut, not
// exposure: the matrix can carry a saturated sensor colour outside the display
// primaries even when the sensor itself did not clip.
void XYZToSRGB8(const std::vector<FP_PRECISION> xyz[3], bool gamma_encode,
                std::vector<unsigned char>* out);

}  // namespace isp
