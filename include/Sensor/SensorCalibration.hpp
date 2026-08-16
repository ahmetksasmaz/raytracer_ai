#pragma once

// Characterising a sensor: what turns its raw channel readings into colour.
//
// This depends only on the sensor's spectral sensitivities, not on any image,
// which is why it is its own program (`sensor_ccm`) rather than a step in the
// pipeline. Run it once per sensor and reuse the result for every frame that
// sensor captures.
//
// Two things come out of it, and they have to come out together.
//
// A white balance is a set of per-channel gains that make a neutral subject
// read neutral. A colour matrix is a 3x3 that maps the sensor's three numbers
// onto CIE XYZ. Both correct for the same thing -- that the sensor's channels
// are not the eye's -- so fitting the matrix without knowing whether the gains
// will be applied first double-corrects the image. The matrix that is right
// after white balancing is not the matrix that is right without it.
//
// So this fits both, from the same illuminant, and writes both to one file. An
// ISP names which one it is using and the two cannot drift apart.

#include <string>

#include "SensorModel.hpp"

namespace sensor_calibration {

struct Calibration {
  // Per-channel gains that take a neutral subject under `white` to equal
  // channel readings, normalised so green is 1.0 (green is the reference
  // because a Bayer CFA has twice as many green sites, so it is the least noisy
  // channel to leave untouched).
  FP_PRECISION wb_gains[3] = {1.0, 1.0, 1.0};

  // sensorRGB -> CIE XYZ, fitted on white-balanced sensor readings.
  FP_PRECISION matrix_after_wb[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

  // sensorRGB -> CIE XYZ, fitted on raw sensor readings, no gains applied.
  // This is the matrix the pipeline used before white balance existed.
  FP_PRECISION matrix_no_wb[3][3] = {{1, 0, 0}, {0, 1, 0}, {0, 0, 1}};

  // RMS over the training reflectances, relative to the mean target magnitude.
  //
  // The residual is part of the result, not a diagnostic: a sensor whose
  // sensitivities are not a linear transform of the CIE matching functions --
  // the Luther condition -- cannot be corrected exactly by any 3x3, and this
  // quantifies how far off it is. The two residuals differ only through the
  // conditioning of the fit, so they should be close; a large gap means one of
  // the channels is being scaled into insignificance.
  FP_PRECISION residual_after_wb = 0.0;
  FP_PRECISION residual_no_wb = 0.0;

  std::string illuminant = "D65";
  int sample_count = 0;

  // True when the fit trained on measured ColorChecker reflectances from the
  // spectral library rather than on the compiled-in sRGB triples uplifted to
  // spectra. The residual only means something about the sensor in the first
  // case; in the second it partly measures the uplift.
  bool measured_training = false;
};

// Fits everything above from the sensor's own sensitivities under `illuminant`.
Calibration Fit(const SensorModel& sensor, const Spectrum& illuminant,
                const std::string& illuminant_name);

bool Write(const std::string& path, const Calibration& calibration,
           std::string* error = nullptr);

// Reads a calibration back. Accepts the older single-matrix file that wrote
// only "sensor_rgb_to_xyz", loading it as matrix_no_wb with unit gains, so a
// RAW captured before the split can still be processed.
bool Read(const std::string& path, Calibration* calibration,
          std::string* error = nullptr);

}  // namespace sensor_calibration
