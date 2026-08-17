// sensor_illum_chroma -- the illumination spectrum, as chromaticity this
// sensor can act on.
//
// The renderer knows exactly what light fell on every pixel and writes it as a
// spectrum. An ISP cannot use a spectrum: it works on three numbers per pixel
// and has never seen a wavelength in its life. This stage is the translation --
// it integrates the illumination cube against the camera's own colour filters
// and reports, per pixel, what that light looks like to THIS sensor:
//
//     r/g  and  b/g       (green is the reference; see below)
//
// That pair is exactly what a von Kries white balance needs, and because it
// comes from the renderer rather than from an estimator, it is ground truth. It
// is what an auto-white-balance algorithm is trying to guess.
//
// Green is the denominator because green is the axis a von Kries correction
// leaves alone: a Bayer CFA samples green twice as often as red or blue, so it
// carries the least noise and is the channel worth not touching.
//
// This is deliberately sensor-side, not renderer-side. Chromaticity is a
// property of the light AND the camera looking at it -- the same illumination
// cube gives different ratios through a Nikon and a Canon. Baking one camera's
// answer into the render would throw away the one-render-many-cameras property
// the whole pipeline exists for.
//
// Note there is no exposure, no aperture, no quantum efficiency and no geometry
// factor here, and deliberately no call to ElectronsFromPhotons. A ratio of two
// channels cancels every scale factor common to both, so applying them would be
// arithmetic that changes nothing while implying this output depends on
// exposure. It does not.

#include <cmath>
#include <cstdio>

#include "Pipeline/Stage.hpp"
#include "Sensor/SensorConfig.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");
  const std::string config = args.Get("config");

  if (in.empty() || out.empty() || config.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <illumination.exr> --out <illumchroma.exr> "
                           "--config <sensor.json> [--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<Spectrum> illumination;
  std::string error;
  if (!pipeline::ReadSpectral(in, &width, &height, &illumination, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const Spectrum sensitivity[3] = {
      sensor.ChannelSensitivity(SensorChannel::kRed),
      sensor.ChannelSensitivity(SensorChannel::kGreen),
      sensor.ChannelSensitivity(SensorChannel::kBlue)};

  const size_t pixel_count = illumination.size();
  std::vector<FP_PRECISION> r_over_g(pixel_count, 1.0);
  std::vector<FP_PRECISION> b_over_g(pixel_count, 1.0);
  std::vector<bool> lit(pixel_count, false);

  // First pass: the ratios wherever there is light to take a ratio of.
  FP_PRECISION sum_r = 0.0, sum_b = 0.0;
  size_t lit_count = 0;
  for (size_t i = 0; i < pixel_count; i++) {
    FP_PRECISION response[3] = {0.0, 0.0, 0.0};
    for (int c = 0; c < 3; c++) {
      for (int band = 0; band < kSpectralBands; band++) {
        response[c] += illumination[i][band] * sensitivity[c][band];
      }
    }
    if (!(response[1] > 1e-30)) continue;

    r_over_g[i] = response[0] / response[1];
    b_over_g[i] = response[2] / response[1];
    lit[i] = true;
    lit_count++;
    sum_r += r_over_g[i];
    sum_b += b_over_g[i];
  }

  // Second pass: an unlit pixel has no illuminant of its own to report, and a
  // ratio of zeros is not a colour. Filling it with the scene's mean
  // chromaticity is the least-wrong answer -- it leaves those pixels neutral
  // relative to everything around them rather than pushing them somewhere
  // arbitrary. The count is printed because a large one means the map is mostly
  // fabricated, which is a fact about the result, not a warning to suppress.
  const FP_PRECISION mean_r = lit_count > 0 ? sum_r / lit_count : 1.0;
  const FP_PRECISION mean_b = lit_count > 0 ? sum_b / lit_count : 1.0;
  for (size_t i = 0; i < pixel_count; i++) {
    if (lit[i]) continue;
    r_over_g[i] = mean_r;
    b_over_g[i] = mean_b;
  }

  if (!pipeline::WritePair(out, width, height, r_over_g, b_over_g, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const double total = static_cast<double>(pixel_count);
  std::printf("%s: %dx%d, mean chromaticity r/g=%.4f b/g=%.4f"
              " -- %.2f%% unlit, filled with the mean -> %s\n",
              args.program().c_str(), width, height,
              static_cast<double>(mean_r), static_cast<double>(mean_b),
              total > 0 ? 100.0 * (total - lit_count) / total : 0.0,
              out.c_str());
  return 0;
}
