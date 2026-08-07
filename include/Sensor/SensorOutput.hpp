#pragma once

// Writes the four products of a sensor render.
//
//   1. spectral radiance cube   -- sensor-INDEPENDENT, at the sensor plane
//   2. RAW Bayer mosaic         -- sensorRGB, mosaiced, in digital numbers
//   3. demosaiced sensorRGB     -- same space, bilinear, for inspection
//   4. sensorRGB -> CIE XYZ 3x3 -- fitted from the sensor's own sensitivities
//
// (1) is the important one. It is written before the light meets the sensor, so
// a single expensive render can be replayed through any number of different
// camera sensors offline instead of being re-rendered per sensor.

#include <array>
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "SensorModel.hpp"
#include "SpectralData.hpp"
#include "Spectrum.hpp"

namespace sensor_output {

// --- 1. Sensor-independent spectral radiance --------------------------------
// Multi-channel EXR, one channel per band, named by wavelength. Channels are
// written in ascending-name order because that is how EXR readers expect them.
bool WriteSpectralCube(const std::string& path, int width, int height,
                       const Spectrum* spectral_image);

// --- 2. RAW Bayer mosaic ----------------------------------------------------
// Single channel of digital numbers. Written as 16-bit binary PGM, which is a
// genuine integer format that any tool can read, and as a float EXR for
// convenience. stb_image_write only emits 8-bit PNG, which would throw away
// most of a 12-bit range, hence PGM.
bool WriteBayerRaw(const std::string& pgm_path, const std::string& exr_path,
                   int width, int height, const std::vector<FP_PRECISION>& dn,
                   int bit_depth);

// --- 3. Demosaiced sensorRGB ------------------------------------------------
// Bilinear demosaic. Kept in sensor space rather than converted to sRGB, since
// the whole point is to study what the sensor actually recorded.
bool WriteDemosaiced(const std::string& path, int width, int height,
                     const std::vector<FP_PRECISION>& dn,
                     const SensorModel& sensor);

// Bilinear demosaic shared by the sensorRGB and sRGB outputs.
void Demosaic(int width, int height, const std::vector<FP_PRECISION>& dn,
              const SensorModel& sensor, std::vector<FP_PRECISION> rgb[3]);

// --- 4. sensorRGB -> CIE XYZ ------------------------------------------------
// Least-squares fit over a set of reflectances under a chosen illuminant.
//
// The sensor's three spectral sensitivities are almost never an exact linear
// transform of the CIE matching functions (the Luther condition), so no 3x3 is
// exactly right. The residual reported alongside the matrix is therefore part
// of the result, not a diagnostic: it quantifies how far this sensor is from
// colorimetric.
struct ColorMatrixFit {
  FP_PRECISION m[3][3];
  FP_PRECISION residual;      // RMS over the training reflectances
  std::string illuminant;
  int sample_count;
  // True when the fit trained on measured ColorChecker reflectances from the
  // spectral library rather than on the compiled-in sRGB triples uplifted to
  // spectra. The residual only means something about the sensor in the first
  // case; in the second it partly measures the uplift.
  bool measured_training = false;
};

ColorMatrixFit FitSensorToXYZ(const SensorModel& sensor,
                              const Spectrum& illuminant,
                              const std::string& illuminant_name);

bool WriteColorMatrix(const std::string& path, const ColorMatrixFit& fit);

// --- 5. Display-ready sRGB --------------------------------------------------
// Demosaic -> sensorRGB -> CIE XYZ (via the fitted matrix above) -> linear sRGB
// -> sRGB transfer function. This is the only output that leaves sensor space,
// and it exists to make the render viewable; the sensorRGB products above are
// the ones to compute on.
//
// Exposure is normalised on a high percentile rather than the maximum so a
// single hot pixel cannot darken the whole image.
bool WriteDemosaicedSRGB(const std::string& path, int width, int height,
                         const std::vector<FP_PRECISION>& dn,
                         const SensorModel& sensor, const ColorMatrixFit& fit);

}  // namespace sensor_output
