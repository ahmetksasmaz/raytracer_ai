#include "SensorOutput.hpp"

#include "SpectrumLibrary.hpp"

#include <algorithm>
#include <cstring>
#include <fstream>

#include "../../extern/stb_image_write.h"
#include "../../extern/tinyexr.h"

namespace sensor_output {
namespace {

// Writes a multi-channel float EXR. tinyexr wants one contiguous plane per
// channel and expects them sorted by channel name.
bool WriteMultiChannelEXR(const std::string& path, int width, int height,
                          const std::vector<std::string>& channel_names,
                          const std::vector<std::vector<float>>& planes) {
  if (channel_names.size() != planes.size() || planes.empty()) return false;

  std::vector<int> order(channel_names.size());
  for (size_t i = 0; i < order.size(); i++) order[i] = static_cast<int>(i);
  std::sort(order.begin(), order.end(), [&](int a, int b) {
    return channel_names[a] < channel_names[b];
  });

  std::vector<float*> plane_ptrs(planes.size());
  for (size_t i = 0; i < order.size(); i++) {
    plane_ptrs[i] = const_cast<float*>(planes[order[i]].data());
  }

  EXRHeader header;
  InitEXRHeader(&header);
  EXRImage image;
  InitEXRImage(&image);

  image.num_channels = static_cast<int>(planes.size());
  image.images = reinterpret_cast<unsigned char**>(plane_ptrs.data());
  image.width = width;
  image.height = height;

  header.num_channels = image.num_channels;
  header.channels = static_cast<EXRChannelInfo*>(
      malloc(sizeof(EXRChannelInfo) * header.num_channels));
  header.pixel_types = static_cast<int*>(malloc(sizeof(int) * header.num_channels));
  header.requested_pixel_types =
      static_cast<int*>(malloc(sizeof(int) * header.num_channels));

  for (int i = 0; i < header.num_channels; i++) {
    const std::string& name = channel_names[order[i]];
    std::strncpy(header.channels[i].name, name.c_str(), 255);
    header.channels[i].name[std::min<size_t>(name.size(), 255)] = '\0';
    header.pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
    // Full float, not half: spectral radiance and digital numbers both span a
    // wide range, and half would quantise the data this whole pipeline exists
    // to preserve.
    header.requested_pixel_types[i] = TINYEXR_PIXELTYPE_FLOAT;
  }

  const char* err = nullptr;
  const int ret = SaveEXRImageToFile(&image, &header, path.c_str(), &err);

  free(header.channels);
  free(header.pixel_types);
  free(header.requested_pixel_types);
  if (ret != TINYEXR_SUCCESS) {
    std::fprintf(stderr, "sensor: failed to write %s: %s\n", path.c_str(),
                 err ? err : "unknown error");
    if (err) FreeEXRErrorMessage(err);
    return false;
  }
  return true;
}

}  // namespace

bool WriteSpectralCube(const std::string& path, int width, int height,
                       const Spectrum* spectral_image) {
  const size_t pixel_count = static_cast<size_t>(width) * height;

  std::vector<std::string> names(kSpectralBands);
  std::vector<std::vector<float>> planes(kSpectralBands);
  for (int band = 0; band < kSpectralBands; band++) {
    // Zero-padded so lexical channel ordering matches wavelength ordering.
    char name[32];
    std::snprintf(name, sizeof(name), "%04.0fnm", BandWavelength(band));
    names[band] = name;
    planes[band].resize(pixel_count);
    for (size_t i = 0; i < pixel_count; i++) {
      planes[band][i] = static_cast<float>(spectral_image[i][band]);
    }
  }
  return WriteMultiChannelEXR(path, width, height, names, planes);
}

bool WriteBayerRaw(const std::string& pgm_path, const std::string& exr_path,
                   int width, int height, const std::vector<FP_PRECISION>& dn,
                   int bit_depth) {
  bool ok = true;

  // 16-bit binary PGM: big-endian sample order per the format spec.
  {
    std::ofstream out(pgm_path, std::ios::binary);
    if (!out) {
      std::fprintf(stderr, "sensor: cannot open %s\n", pgm_path.c_str());
      ok = false;
    } else {
      const int max_value = (1 << bit_depth) - 1;
      out << "P5\n"
          << "# RAW Bayer mosaic in sensorRGB, " << bit_depth << "-bit range\n"
          << width << " " << height << "\n"
          << max_value << "\n";
      for (size_t i = 0; i < dn.size(); i++) {
        const int v = std::min(std::max(static_cast<int>(dn[i]), 0), max_value);
        const unsigned char bytes[2] = {static_cast<unsigned char>(v >> 8),
                                        static_cast<unsigned char>(v & 0xFF)};
        out.write(reinterpret_cast<const char*>(bytes), 2);
      }
    }
  }

  std::vector<std::vector<float>> plane(1);
  plane[0].resize(dn.size());
  for (size_t i = 0; i < dn.size(); i++) plane[0][i] = static_cast<float>(dn[i]);
  ok &= WriteMultiChannelEXR(exr_path, width, height, {"Y"}, plane);
  return ok;
}

void Demosaic(int width, int height, const std::vector<FP_PRECISION>& dn,
              const SensorModel& sensor, std::vector<FP_PRECISION> rgb[3]) {
  const size_t pixel_count = static_cast<size_t>(width) * height;
  for (int c = 0; c < 3; c++) rgb[c].assign(pixel_count, 0.0);

  // For each output pixel and channel, take the pixel's own value if the CFA
  // put that colour there, otherwise average the neighbours that carry it.
  // Deliberately simple -- this exists to make the mosaic viewable, not to be a
  // demosaic study.
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const size_t index = static_cast<size_t>(y) * width + x;
      for (int c = 0; c < 3; c++) {
        const SensorChannel want = static_cast<SensorChannel>(c);
        if (sensor.ChannelAt(x, y) == want) {
          rgb[c][index] = dn[index];
          continue;
        }
        FP_PRECISION sum = 0.0;
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            if (sensor.ChannelAt(nx, ny) != want) continue;
            sum += dn[static_cast<size_t>(ny) * width + nx];
            count++;
          }
        }
        rgb[c][index] = count > 0 ? sum / count : 0.0;
      }
    }
  }
}

bool WriteDemosaiced(const std::string& path, int width, int height,
                     const std::vector<FP_PRECISION>& dn,
                     const SensorModel& sensor) {
  std::vector<FP_PRECISION> rgb[3];
  Demosaic(width, height, dn, sensor, rgb);
  std::vector<std::vector<float>> planes(3);
  for (int c = 0; c < 3; c++) {
    planes[c].resize(rgb[c].size());
    for (size_t i = 0; i < rgb[c].size(); i++) planes[c][i] = static_cast<float>(rgb[c][i]);
  }
  return WriteMultiChannelEXR(path, width, height, {"R", "G", "B"}, planes);
}

bool WriteDemosaicedSRGB(const std::string& path, int width, int height,
                         const std::vector<FP_PRECISION>& dn,
                         const SensorModel& sensor, const ColorMatrixFit& fit) {
  const size_t pixel_count = static_cast<size_t>(width) * height;
  std::vector<FP_PRECISION> rgb[3];
  Demosaic(width, height, dn, sensor, rgb);

  // STEP 1: fixed sensor response. Each channel's digital number maps onto
  // [0,1] against the ADC's own full scale and the declared dynamic range --
  // constants of the sensor, never of the frame. Nothing here inspects the
  // image, so the same subject under the same light produces the same code
  // values whatever else is in shot.
  //
  // Clipping is counted rather than avoided. A camera that cannot fit the
  // scene's range blows highlights and crushes shadows; that is the measurement,
  // and the counts are reported so it is visible without opening the file.
  size_t over = 0, under = 0;
  const FP_PRECISION white_dn = sensor.MaxDN();
  const FP_PRECISION black_dn = sensor.BlackDN();

  std::vector<FP_PRECISION> lin(pixel_count * 3, 0.0);
  for (size_t i = 0; i < pixel_count; i++) {
    FP_PRECISION channel[3];
    for (int c = 0; c < 3; c++) {
      const FP_PRECISION dn = rgb[c][i];
      if (dn >= white_dn) over++;
      else if (dn <= black_dn) under++;
      channel[c] = sensor.NormalizedFromDN(dn);
    }

    // STEP 2: sensor space -> CIE XYZ -> linear sRGB, after per-channel
    // clipping, so a saturated channel shifts hue the way a real one does.
    const FP_PRECISION X = fit.m[0][0]*channel[0] + fit.m[0][1]*channel[1] + fit.m[0][2]*channel[2];
    const FP_PRECISION Y = fit.m[1][0]*channel[0] + fit.m[1][1]*channel[1] + fit.m[1][2]*channel[2];
    const FP_PRECISION Z = fit.m[2][0]*channel[0] + fit.m[2][1]*channel[1] + fit.m[2][2]*channel[2];
    const Vec3f c = XYZToLinearSRGB(X, Y, Z);
    lin[i*3+0] = c.x; lin[i*3+1] = c.y; lin[i*3+2] = c.z;
  }

  // STEP 3: encode and quantise. The clamp here is the sRGB gamut, not
  // exposure: the matrix can carry a saturated sensor colour outside the
  // display primaries even when the sensor itself did not clip.
  std::vector<unsigned char> out(pixel_count * 3);
  for (size_t i = 0; i < pixel_count * 3; i++) {
    const FP_PRECISION v = std::min(std::max(lin[i], static_cast<FP_PRECISION>(0.0)),
                                    static_cast<FP_PRECISION>(1.0));
    // sRGB transfer function (not a plain 2.2 gamma).
    const FP_PRECISION e = (v <= 0.0031308) ? (12.92 * v)
                                            : (1.055 * std::pow(v, 1.0/2.4) - 0.055);
    out[i] = static_cast<unsigned char>(std::lround(e * 255.0));
  }

  const double samples = static_cast<double>(pixel_count) * 3.0;
  std::printf("sensor: %.1f stops, saturation at %.0f DN, black at %.2f DN"
              " -- %.2f%% clipped, %.2f%% below black\n",
              static_cast<double>(sensor.DynamicRangeStops()),
              static_cast<double>(white_dn), static_cast<double>(black_dn),
              100.0 * over / samples, 100.0 * under / samples);

  return stbi_write_png(path.c_str(), width, height, 3, out.data(), width*3) != 0;
}

ColorMatrixFit FitSensorToXYZ(const SensorModel& sensor,
                              const Spectrum& illuminant,
                              const std::string& illuminant_name) {
  // Build training pairs: for each reflectance, what the sensor records and
  // what the standard observer sees, both under the same illuminant.
  std::vector<std::array<FP_PRECISION, 3>> sensor_rgb, target_xyz;

  const Spectrum sens_r = sensor.ChannelSensitivity(SensorChannel::kRed);
  const Spectrum sens_g = sensor.ChannelSensitivity(SensorChannel::kGreen);
  const Spectrum sens_b = sensor.ChannelSensitivity(SensorChannel::kBlue);

  // Prefer measured ColorChecker reflectances from the spectral library. The
  // compiled-in table is published sRGB uplifted to a spectrum, and an uplifted
  // spectrum is only one of infinitely many metamers -- fitting against it
  // measures the uplift as much as the sensor. Falling back to it keeps scenes
  // that ship without the library working.
  std::vector<Spectrum> reflectances;
  const SpectrumLibrary& library = SpectrumLibrary::Instance();
  for (int i = 0; i < spectral_data::kColorCheckerCount; i++) {
    const auto& patch = spectral_data::kColorCheckerSRGB[i];
    if (const SpectrumRecord* measured =
            library.Find(std::string("material:") + patch.name)) {
      if (!measured->multichannel) {
        reflectances.push_back(measured->value);
        continue;
      }
    }
    reflectances.clear();
    break;
  }

  const bool measured_training = !reflectances.empty();
  if (!measured_training) {
    for (int i = 0; i < spectral_data::kColorCheckerCount; i++) {
      const auto& patch = spectral_data::kColorCheckerSRGB[i];
      reflectances.push_back(UpliftRGB(Vec3f{patch.r, patch.g, patch.b}));
    }
  }

  for (size_t i = 0; i < reflectances.size(); i++) {
    const Spectrum stimulus = hadamard(reflectances[i], illuminant);

    std::array<FP_PRECISION, 3> rgb{0, 0, 0};
    for (int band = 0; band < kSpectralBands; band++) {
      rgb[0] += stimulus[band] * sens_r[band];
      rgb[1] += stimulus[band] * sens_g[band];
      rgb[2] += stimulus[band] * sens_b[band];
    }
    sensor_rgb.push_back(rgb);

    FP_PRECISION X, Y, Z;
    SpectrumToXYZ(stimulus, X, Y, Z);
    target_xyz.push_back({X, Y, Z});
  }

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

  ColorMatrixFit fit;
  fit.illuminant = illuminant_name;
  fit.sample_count = spectral_data::kColorCheckerCount;
  fit.measured_training = measured_training;

  if (std::fabs(det) < 1e-30) {
    // Degenerate sensor (e.g. two identical channels): report identity and a
    // residual that makes the failure obvious rather than returning garbage.
    for (int i = 0; i < 3; i++)
      for (int j = 0; j < 3; j++) fit.m[i][j] = (i == j) ? 1.0 : 0.0;
    fit.residual = -1.0;
    return fit;
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
      for (int i = 0; i < 3; i++) atb[i] += sensor_rgb[s][i] * target_xyz[s][row];
    }
    for (int i = 0; i < 3; i++) {
      fit.m[row][i] =
          inv[i][0] * atb[0] + inv[i][1] * atb[1] + inv[i][2] * atb[2];
    }
  }

  // RMS residual, relative to the mean target magnitude so it reads as a
  // fraction rather than in arbitrary units.
  FP_PRECISION sq_error = 0.0, sq_target = 0.0;
  for (size_t s = 0; s < sensor_rgb.size(); s++) {
    for (int row = 0; row < 3; row++) {
      FP_PRECISION predicted = 0.0;
      for (int i = 0; i < 3; i++) predicted += fit.m[row][i] * sensor_rgb[s][i];
      const FP_PRECISION d = predicted - target_xyz[s][row];
      sq_error += d * d;
      sq_target += target_xyz[s][row] * target_xyz[s][row];
    }
  }
  fit.residual = sq_target > 0 ? std::sqrt(sq_error / sq_target) : 0.0;

  // Fix the matrix's absolute scale by anchoring full scale to white.
  //
  // The fit is computed in whatever units the training stimulus happened to be
  // in, so the result is only determined up to a scale factor. That is fine for
  // the residual, which is relative, but not for a display path with a fixed
  // exposure: the image would land at an arbitrary brightness. Scaling so that
  // a sensor reading of (1,1,1) -- every channel at saturation -- maps to
  // luminance 1 makes the two ends agree, which is exactly the convention the
  // rest of the pipeline needs: a fully exposed neutral is white.
  //
  // Only the overall gain changes, so the residual computed above still stands.
  const FP_PRECISION y_at_full_scale = fit.m[1][0] + fit.m[1][1] + fit.m[1][2];
  if (y_at_full_scale > 1e-30) {
    const FP_PRECISION normalise = 1.0 / y_at_full_scale;
    for (int row = 0; row < 3; row++) {
      for (int i = 0; i < 3; i++) fit.m[row][i] *= normalise;
    }
  }
  return fit;
}

bool WriteColorMatrix(const std::string& path, const ColorMatrixFit& fit) {
  std::ofstream out(path);
  if (!out) {
    std::fprintf(stderr, "sensor: cannot open %s\n", path.c_str());
    return false;
  }
  out.precision(9);
  out << "{\n";
  out << "  \"_comment\": \"Least-squares sensorRGB -> CIE XYZ, scaled so that "
         "sensorRGB (1,1,1) at full scale gives Y=1. Apply it to sensorRGB "
         "normalised against the ADC full scale, not to raw digital numbers. "
         "The residual is part of the "
         "result: a sensor whose sensitivities are not a linear transform of "
         "the CIE matching functions (the Luther condition) cannot be "
         "corrected exactly by any 3x3, and this quantifies how far off it "
         "is.\",\n";
  out << "  \"illuminant\": \"" << fit.illuminant << "\",\n";
  out << "  \"training_samples\": " << fit.sample_count << ",\n";
  out << "  \"training_reflectances\": \""
      << (fit.measured_training
              ? "measured ColorChecker reflectance from spectra/materials/"
                "colorchecker.spd"
              : "ColorChecker approximated by uplifting published sRGB, NOT "
                "measured spectra -- put spectra/ on the search path to fit "
                "against real measurements")
      << "\",\n";
  out << "  \"relative_rms_residual\": " << fit.residual << ",\n";
  out << "  \"sensor_rgb_to_xyz\": [\n";
  for (int i = 0; i < 3; i++) {
    out << "    [" << fit.m[i][0] << ", " << fit.m[i][1] << ", " << fit.m[i][2]
        << "]" << (i < 2 ? "," : "") << "\n";
  }
  out << "  ]\n}\n";
  return true;
}

}  // namespace sensor_output
