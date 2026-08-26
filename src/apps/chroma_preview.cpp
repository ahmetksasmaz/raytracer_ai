// chroma_preview -- look at a ground-truth illuminant map.
//
// sensor_illum_chroma writes the illuminant at every pixel as two ratios, r/g
// and b/g. That is the right form to compute with -- it is exactly what a von
// Kries correction divides by -- but two channels named r_over_g and b_over_g
// are not something any viewer can display, so the one product in the pipeline
// that represents "what light was here" is also the one you cannot look at.
//
// This reconstructs the triple the ratios came from,
//
//     (r/g, 1, b/g)
//
// which IS the illuminant's colour in sensor space, and makes it viewable.
//
// Two things have to be decided to draw it, and both are chosen so the picture
// does not mislead:
//
// BRIGHTNESS. A chromaticity carries none -- only ratios survive. Normalising
// each pixel on its own would flatten the map to pure hue and hide that one
// side of a scene may be lit far more strongly in one channel than the other.
// So the whole image is scaled by a single factor, from its own maximum, and
// that factor is printed. It is a viewing gain, nothing more.
//
// COLOUR. The default is sensor space, unconverted, and that is the view worth
// having: it is literally the divisor, so
//
//     demosaiced / (r/g, 1, b/g)  =  white balanced
//
// holds pixel by pixel, and the picture comes out green-cyan the way published
// illuminant maps do -- green is the reference channel and a CFA's green
// response dominates, so r/g and b/g both sit below 1.
//
// --calibration instead pushes the triple through the sensor's matrix to CIE
// XYZ and then sRGB, which answers a different question: what colour the light
// would look like to the eye, tungsten orange and daylight neutral. Useful, but
// no longer the divisor. The matrix wanted there is matrix_no_wb, since this
// data has NOT been white balanced -- it is the thing a white balance is
// derived from.
//
// --exr writes the reconstructed triple as a three-channel image, so the
// division relation above can be asserted rather than eyeballed.

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "ISP/ISP.hpp"
#include "ImageIO/ImageIO.hpp"
#include "Pipeline/Stage.hpp"
#include "Sensor/SensorCalibration.hpp"
#include "Spectrum.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");

  if (in.empty() || out.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <illumchroma.exr> --out <preview.png> "
                           "[--calibration <ccm.json>] [--linear] [--exr <triple.exr>]");
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> r_over_g, b_over_g;
  std::string error;
  if (!pipeline::ReadPair(in, &width, &height, &r_over_g, &b_over_g, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const size_t pixel_count = r_over_g.size();
  std::vector<FP_PRECISION> rgb[3];
  for (int c = 0; c < 3; c++) rgb[c].assign(pixel_count, 1.0);
  for (size_t i = 0; i < pixel_count; i++) {
    rgb[0][i] = r_over_g[i];
    rgb[2][i] = b_over_g[i];
  }

  // Optional: sensor space -> CIE XYZ -> linear sRGB, so the hues are the
  // eye's. matrix_no_wb, because this data is unbalanced by definition.
  std::string space = "sensor space (uncorrected)";
  if (args.Has("calibration")) {
    sensor_calibration::Calibration calibration;
    if (!sensor_calibration::Read(args.Get("calibration"), &calibration,
                                  &error)) {
      return pipeline::Fail(args.program(), error);
    }
    std::vector<FP_PRECISION> xyz[3];
    isp::ApplyColorMatrix(rgb, calibration.matrix_no_wb, xyz);
    for (size_t i = 0; i < pixel_count; i++) {
      const Vec3f linear = XYZToLinearSRGB(xyz[0][i], xyz[1][i], xyz[2][i]);
      rgb[0][i] = linear.x;
      rgb[1][i] = linear.y;
      rgb[2][i] = linear.z;
    }
    space = "sRGB via " + calibration.illuminant + " matrix_no_wb";
  }

  // One scale for the whole image, so spatial variation survives.
  FP_PRECISION peak = 0.0;
  for (int c = 0; c < 3; c++) {
    for (FP_PRECISION v : rgb[c]) peak = std::max(peak, v);
  }
  const FP_PRECISION scale = peak > 1e-30 ? 1.0 / peak : 1.0;

  const bool gamma_encode = !args.Has("linear");
  std::vector<unsigned char> pixels(pixel_count * 3, 0);
  for (size_t i = 0; i < pixel_count; i++) {
    for (int c = 0; c < 3; c++) {
      const FP_PRECISION v = rgb[c][i] * scale;
      pixels[i * 3 + c] =
          isp::Quantise8(gamma_encode ? isp::EncodeSRGB(v) : v);
    }
  }

  if (!image_io::WritePNG8(out, width, height, 3, pixels, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  // The triple as data, unscaled and untransformed: exactly what
  // isp_whitebalance --chroma divides by.
  if (args.Has("exr")) {
    std::vector<FP_PRECISION> triple[3];
    triple[0] = r_over_g;
    triple[1].assign(pixel_count, 1.0);
    triple[2] = b_over_g;
    if (!pipeline::WriteTriple(args.Get("exr"), width, height, triple,
                               &error)) {
      return pipeline::Fail(args.program(), error);
    }
  }

  // The spread says whether the map is worth looking at: a single-illuminant
  // scene is flat, and a mixed one is not.
  //
  // Reported as 1st..99th percentile, not min..max. A ratio is ill-conditioned
  // wherever the green response is near zero -- a handful of pixels at patch
  // edges -- and those few dominate the extremes. On a D65 chart the true
  // min..max spans 0.23..0.88 while p1..p99 is flat to three digits, so min/max
  // would suggest variation that is not there.
  auto percentiles = [&](std::vector<FP_PRECISION> v,
                         FP_PRECISION* lo, FP_PRECISION* hi) {
    std::sort(v.begin(), v.end());
    *lo = v[v.size() / 100];
    *hi = v[v.size() * 99 / 100];
  };
  FP_PRECISION lo_r, hi_r, lo_b, hi_b;
  percentiles(r_over_g, &lo_r, &hi_r);
  percentiles(b_over_g, &lo_b, &hi_b);
  std::printf("%s: %dx%d, %s, viewing gain %.3f"
              " -- r/g %.3f..%.3f, b/g %.3f..%.3f (p1..p99) -> %s\n",
              args.program().c_str(), width, height, space.c_str(),
              static_cast<double>(scale), static_cast<double>(lo_r),
              static_cast<double>(hi_r), static_cast<double>(lo_b),
              static_cast<double>(hi_b), out.c_str());
  return 0;
}
