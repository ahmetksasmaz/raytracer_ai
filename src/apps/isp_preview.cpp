// isp_preview -- look at a three-channel linear image without changing its
// colour.
//
// Every other way of turning pipeline data into a PNG applies a colour
// transform on the way: isp_srgb multiplies by the sRGB primaries matrix, and
// raw_preview reads only the single-channel RAW. Neither can show you what
// _demosaiced.exr or _wb.exr actually contain.
//
// That matters because those two files are the interesting middle of the ISP.
// _demosaiced.exr is sensor space with the camera's own colour cast intact;
// _wb.exr is the same data after white balance, so a neutral subject reads
// neutral but the numbers are still the sensor's, not the eye's. Pushing either
// through isp_srgb would produce a plausible-looking and wrong picture, because
// sensorRGB is not linear sRGB.
//
// So this stage does the minimum: clamp, optionally apply the sRGB transfer
// function, quantise. No matrix, no white balance, no exposure.
//
//   --linear    skip the transfer function. Truer to the numbers, but it
//               buries the shadows, so it is not the default.
//   --scale N   divide by N before encoding. White balance leaves green at 1
//               and pushes red and blue above it, so a saturated patch can
//               exceed 1.0 and clip; --scale lets you see it. Default 1.0,
//               because the clipped version is the honest one.

#include <cstdio>

#include "ISP/ISP.hpp"
#include "ImageIO/ImageIO.hpp"
#include "Pipeline/Stage.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");

  if (in.empty() || out.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <linear3.exr> --out <preview.png> "
                           "[--linear] [--scale N]");
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> rgb[3];
  std::string error;
  if (!pipeline::ReadTriple(in, &width, &height, rgb, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const bool gamma_encode = !args.Has("linear");
  const FP_PRECISION scale = args.Number("scale", 1.0);
  if (!(scale > 0.0)) {
    return pipeline::Fail(args.program(), "--scale must be positive");
  }

  const size_t pixel_count = static_cast<size_t>(width) * height;
  std::vector<unsigned char> pixels(pixel_count * 3, 0);

  // Count what the encoding clips, rather than letting it happen quietly. On a
  // white-balanced image this is the honest measure of how far the gains pushed
  // red and blue past full scale.
  size_t clipped = 0;
  for (size_t i = 0; i < pixel_count; i++) {
    for (int c = 0; c < 3; c++) {
      const FP_PRECISION value = rgb[c][i] / scale;
      if (value > 1.0) clipped++;
      pixels[i * 3 + c] =
          isp::Quantise8(gamma_encode ? isp::EncodeSRGB(value) : value);
    }
  }

  if (!image_io::WritePNG8(out, width, height, 3, pixels, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const double samples = static_cast<double>(pixel_count) * 3.0;
  std::printf("%s: %dx%d, %s, scale %.3f -- %.2f%% clipped,"
              " no colour transform -> %s\n",
              args.program().c_str(), width, height,
              gamma_encode ? "sRGB transfer function" : "linear",
              static_cast<double>(scale),
              samples > 0 ? 100.0 * clipped / samples : 0.0, out.c_str());
  return 0;
}
