#include "SensorCalibration.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <vector>

#include "SpectralData.hpp"
#include "SpectrumLibrary.hpp"

#include "../../extern/json.hpp"

namespace sensor_calibration {
namespace {

// The ColorChecker reflectances the fit trains on.
//
// Measured curves from the spectral library are strongly preferred. The
// compiled-in table is published sRGB uplifted to a spectrum, and an uplifted
// spectrum is only one of infinitely many metamers -- fitting against it
// measures the uplift as much as it measures the sensor, which flatters every
// camera. Falling back to it keeps a checkout without spectra/ working.
std::vector<Spectrum> TrainingReflectances(bool* measured) {
  std::vector<Spectrum> reflectances;
  const SpectrumLibrary& library = SpectrumLibrary::Instance();
  for (int i = 0; i < spectral_data::kColorCheckerCount; i++) {
    const auto& patch = spectral_data::kColorCheckerSRGB[i];
    if (const SpectrumRecord* found =
            library.Find(std::string("material:") + patch.name)) {
      if (!found->multichannel) {
        reflectances.push_back(found->value);
        continue;
      }
    }
    reflectances.clear();
    break;
  }

  *measured = !reflectances.empty();
  if (!*measured) {
    for (int i = 0; i < spectral_data::kColorCheckerCount; i++) {
      const auto& patch = spectral_data::kColorCheckerSRGB[i];
      reflectances.push_back(UpliftRGB(Vec3f{patch.r, patch.g, patch.b}));
    }
  }
  return reflectances;
}

// Least-squares sensorRGB -> XYZ over the supplied training pairs, with the
// result scaled so that `anchor` maps to luminance 1.
//
// The fit is computed in whatever units the training stimulus happened to be
// in, so it is only determined up to a scale factor. That is fine for the
// residual, which is relative, but not for a display path with a fixed
// exposure: the image would land at an arbitrary brightness.
//
// `anchor` is what a full-scale neutral looks like in the space this matrix
// consumes. For a matrix applied to raw sensor readings that is (1,1,1). For
// one applied after white balance it is the gain vector, because a raw neutral
// at full scale arrives at this matrix already multiplied by the gains.
//
// Getting that distinction wrong is not obvious: both choices produce a
// perfectly plausible image, differing only by an overall brightness factor of
// a few percent, and the pipeline's exposure is fixed precisely so that such a
// factor is meaningful rather than absorbed downstream.
void SolveMatrix(const std::vector<std::array<FP_PRECISION, 3>>& sensor_rgb,
                 const std::vector<std::array<FP_PRECISION, 3>>& target_xyz,
                 const FP_PRECISION anchor[3], FP_PRECISION m[3][3],
                 FP_PRECISION* residual) {
  // Solve the 3x3 normal equations independently for each XYZ row:
  //   (A^T A) w = A^T t,   A = sensor responses, t = target component.
  FP_PRECISION ata[3][3] = {{0}};
  for (const auto& rgb : sensor_rgb) {
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) ata[i][j] += rgb[i] * rgb[j];
  }

  // Explicit 3x3 inverse via cofactors; the system is tiny and well
  // conditioned for any sensible sensor.
  const FP_PRECISION det =
      ata[0][0] * (ata[1][1] * ata[2][2] - ata[1][2] * ata[2][1]) -
      ata[0][1] * (ata[1][0] * ata[2][2] - ata[1][2] * ata[2][0]) +
      ata[0][2] * (ata[1][0] * ata[2][1] - ata[1][1] * ata[2][0]);

  if (std::fabs(det) < 1e-30) {
    // Degenerate sensor (e.g. two identical channels): report identity and a
    // residual that makes the failure obvious rather than returning garbage.
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) m[i][j] = (i == j) ? 1.0 : 0.0;
    *residual = -1.0;
    return;
  }

  FP_PRECISION inv[3][3];
  const FP_PRECISION inv_det = 1.0 / det;
  inv[0][0] = (ata[1][1] * ata[2][2] - ata[1][2] * ata[2][1]) * inv_det;
  inv[0][1] = (ata[0][2] * ata[2][1] - ata[0][1] * ata[2][2]) * inv_det;
  inv[0][2] = (ata[0][1] * ata[1][2] - ata[0][2] * ata[1][1]) * inv_det;
  inv[1][0] = (ata[1][2] * ata[2][0] - ata[1][0] * ata[2][2]) * inv_det;
  inv[1][1] = (ata[0][0] * ata[2][2] - ata[0][2] * ata[2][0]) * inv_det;
  inv[1][2] = (ata[0][2] * ata[1][0] - ata[0][0] * ata[1][2]) * inv_det;
  inv[2][0] = (ata[1][0] * ata[2][1] - ata[1][1] * ata[2][0]) * inv_det;
  inv[2][1] = (ata[0][1] * ata[2][0] - ata[0][0] * ata[2][1]) * inv_det;
  inv[2][2] = (ata[0][0] * ata[1][1] - ata[0][1] * ata[1][0]) * inv_det;

  for (int row = 0; row < 3; row++) {
    FP_PRECISION atb[3] = {0, 0, 0};
    for (size_t s = 0; s < sensor_rgb.size(); s++) {
      for (int i = 0; i < 3; i++) {
        atb[i] += sensor_rgb[s][i] * target_xyz[s][row];
      }
    }
    for (int i = 0; i < 3; i++) {
      m[row][i] = inv[i][0] * atb[0] + inv[i][1] * atb[1] + inv[i][2] * atb[2];
    }
  }

  // RMS residual, relative to the mean target magnitude so it reads as a
  // fraction rather than in arbitrary units.
  FP_PRECISION sq_error = 0.0, sq_target = 0.0;
  for (size_t s = 0; s < sensor_rgb.size(); s++) {
    for (int row = 0; row < 3; row++) {
      FP_PRECISION predicted = 0.0;
      for (int i = 0; i < 3; i++) predicted += m[row][i] * sensor_rgb[s][i];
      const FP_PRECISION d = predicted - target_xyz[s][row];
      sq_error += d * d;
      sq_target += target_xyz[s][row] * target_xyz[s][row];
    }
  }
  *residual = sq_target > 0 ? std::sqrt(sq_error / sq_target) : 0.0;

  // Only the overall gain changes here, so the residual computed above stands.
  const FP_PRECISION y_at_full_scale =
      m[1][0] * anchor[0] + m[1][1] * anchor[1] + m[1][2] * anchor[2];
  if (y_at_full_scale > 1e-30) {
    const FP_PRECISION normalise = 1.0 / y_at_full_scale;
    for (int row = 0; row < 3; row++) {
      for (int i = 0; i < 3; i++) m[row][i] *= normalise;
    }
  }
}

void ReadMatrix(const nlohmann::json& node, FP_PRECISION m[3][3]) {
  for (int i = 0; i < 3; i++) {
    for (int j = 0; j < 3; j++) {
      m[i][j] = node[i][j].get<FP_PRECISION>();
    }
  }
}

void WriteMatrix(std::ofstream& out, const char* key,
                 const FP_PRECISION m[3][3]) {
  out << "  \"" << key << "\": [\n";
  for (int i = 0; i < 3; i++) {
    out << "    [" << m[i][0] << ", " << m[i][1] << ", " << m[i][2] << "]"
        << (i < 2 ? "," : "") << "\n";
  }
  out << "  ],\n";
}

}  // namespace

Calibration Fit(const SensorModel& sensor, const Spectrum& illuminant,
                const std::string& illuminant_name) {
  Calibration calibration;
  calibration.illuminant = illuminant_name;
  calibration.sample_count = spectral_data::kColorCheckerCount;

  const Spectrum sensitivity[3] = {
      sensor.ChannelSensitivity(SensorChannel::kRed),
      sensor.ChannelSensitivity(SensorChannel::kGreen),
      sensor.ChannelSensitivity(SensorChannel::kBlue)};

  // The white balance gains: what the sensor reads looking at a perfect
  // (spectrally flat, unit) reflector under this illuminant, inverted. Green is
  // pinned at 1.0 rather than normalising by the maximum, because a Bayer CFA
  // samples green twice as often and so carries the least noise -- scaling the
  // other two up to meet it adds less noise than scaling green down.
  FP_PRECISION white_response[3] = {0, 0, 0};
  for (int c = 0; c < 3; c++) {
    for (int band = 0; band < kSpectralBands; band++) {
      white_response[c] += illuminant[band] * sensitivity[c][band];
    }
  }
  for (int c = 0; c < 3; c++) {
    calibration.wb_gains[c] =
        white_response[c] > 1e-30 ? white_response[1] / white_response[c] : 1.0;
  }

  bool measured = false;
  const std::vector<Spectrum> reflectances = TrainingReflectances(&measured);
  calibration.measured_training = measured;

  std::vector<std::array<FP_PRECISION, 3>> raw_rgb, balanced_rgb, target_xyz;
  for (size_t i = 0; i < reflectances.size(); i++) {
    const Spectrum stimulus = hadamard(reflectances[i], illuminant);

    std::array<FP_PRECISION, 3> rgb{0, 0, 0};
    for (int band = 0; band < kSpectralBands; band++) {
      for (int c = 0; c < 3; c++) {
        rgb[c] += stimulus[band] * sensitivity[c][band];
      }
    }
    raw_rgb.push_back(rgb);

    std::array<FP_PRECISION, 3> balanced{rgb[0] * calibration.wb_gains[0],
                                         rgb[1] * calibration.wb_gains[1],
                                         rgb[2] * calibration.wb_gains[2]};
    balanced_rgb.push_back(balanced);

    FP_PRECISION X, Y, Z;
    SpectrumToXYZ(stimulus, X, Y, Z);
    target_xyz.push_back({X, Y, Z});
  }

  // A full-scale raw neutral is (1,1,1) going into matrix_no_wb, and the same
  // neutral arrives at matrix_after_wb already multiplied by the gains. Using
  // the matching anchor for each is what makes the two routes through the ISP
  // -- gains then matrix_after_wb, or matrix_no_wb alone -- produce the same
  // XYZ, which `wb-ccm-pairing` asserts. With (1,1,1) for both they differed by
  // a few percent of overall brightness.
  const FP_PRECISION neutral[3] = {1.0, 1.0, 1.0};
  SolveMatrix(raw_rgb, target_xyz, neutral, calibration.matrix_no_wb,
              &calibration.residual_no_wb);
  SolveMatrix(balanced_rgb, target_xyz, calibration.wb_gains,
              calibration.matrix_after_wb, &calibration.residual_after_wb);
  return calibration;
}

bool Write(const std::string& path, const Calibration& calibration,
           std::string* error) {
  std::ofstream out(path);
  if (!out) {
    if (error) *error = "cannot open " + path;
    return false;
  }
  out.precision(9);
  out << "{\n";
  out << "  \"_comment\": \"Sensor colour calibration. 'wb_gains' are "
         "per-channel gains taking a neutral subject under the named "
         "illuminant to equal readings, green pinned at 1. Each matrix maps "
         "sensorRGB to CIE XYZ and is scaled so full-scale neutral gives Y=1; "
         "apply it to sensorRGB normalised against the ADC full scale, not to "
         "raw digital numbers. Use matrix_after_wb if the gains were applied "
         "and matrix_no_wb if they were not -- using the wrong one "
         "double-corrects or under-corrects the image. The residual is part of "
         "the result: a sensor whose sensitivities are not a linear transform "
         "of the CIE matching functions (the Luther condition) cannot be "
         "corrected exactly by any 3x3, and this quantifies how far off it "
         "is.\",\n";
  out << "  \"illuminant\": \"" << calibration.illuminant << "\",\n";
  out << "  \"training_samples\": " << calibration.sample_count << ",\n";
  out << "  \"training_reflectances\": \""
      << (calibration.measured_training
              ? "measured ColorChecker reflectance from spectra/materials/"
                "colorchecker.spd"
              : "ColorChecker approximated by uplifting published sRGB, NOT "
                "measured spectra -- put spectra/ on the search path to fit "
                "against real measurements")
      << "\",\n";
  out << "  \"wb_gains\": [" << calibration.wb_gains[0] << ", "
      << calibration.wb_gains[1] << ", " << calibration.wb_gains[2] << "],\n";
  out << "  \"relative_rms_residual_after_wb\": "
      << calibration.residual_after_wb << ",\n";
  out << "  \"relative_rms_residual_no_wb\": " << calibration.residual_no_wb
      << ",\n";
  WriteMatrix(out, "matrix_after_wb", calibration.matrix_after_wb);
  WriteMatrix(out, "matrix_no_wb", calibration.matrix_no_wb);
  // Kept under its old name so anything reading the previous single-matrix
  // sidecar still finds what it expects.
  out << "  \"sensor_rgb_to_xyz\": [\n";
  for (int i = 0; i < 3; i++) {
    out << "    [" << calibration.matrix_no_wb[i][0] << ", "
        << calibration.matrix_no_wb[i][1] << ", "
        << calibration.matrix_no_wb[i][2] << "]" << (i < 2 ? "," : "") << "\n";
  }
  out << "  ]\n}\n";
  return static_cast<bool>(out);
}

bool Read(const std::string& path, Calibration* calibration,
          std::string* error) {
  std::ifstream in(path);
  if (!in) {
    if (error) *error = "cannot open " + path;
    return false;
  }

  nlohmann::json json;
  try {
    in >> json;
  } catch (const std::exception& e) {
    if (error) *error = path + " is not valid JSON: " + e.what();
    return false;
  }

  if (json.contains("illuminant")) {
    calibration->illuminant = json["illuminant"].get<std::string>();
  }
  if (json.contains("training_samples")) {
    calibration->sample_count = json["training_samples"].get<int>();
  }
  if (json.contains("wb_gains")) {
    for (int c = 0; c < 3; c++) {
      calibration->wb_gains[c] = json["wb_gains"][c].get<FP_PRECISION>();
    }
  }
  if (json.contains("relative_rms_residual_after_wb")) {
    calibration->residual_after_wb =
        json["relative_rms_residual_after_wb"].get<FP_PRECISION>();
  }
  if (json.contains("relative_rms_residual_no_wb")) {
    calibration->residual_no_wb =
        json["relative_rms_residual_no_wb"].get<FP_PRECISION>();
  }

  if (json.contains("matrix_after_wb")) {
    ReadMatrix(json["matrix_after_wb"], calibration->matrix_after_wb);
  }
  if (json.contains("matrix_no_wb")) {
    ReadMatrix(json["matrix_no_wb"], calibration->matrix_no_wb);
  } else if (json.contains("sensor_rgb_to_xyz")) {
    // The older single-matrix sidecar, written before white balance existed.
    // It was fitted on unbalanced readings, so that is what it is.
    ReadMatrix(json["sensor_rgb_to_xyz"], calibration->matrix_no_wb);
    if (!json.contains("matrix_after_wb")) {
      ReadMatrix(json["sensor_rgb_to_xyz"], calibration->matrix_after_wb);
    }
  } else {
    if (error) *error = path + " contains no colour matrix";
    return false;
  }
  return true;
}

}  // namespace sensor_calibration
