// isp_srgb -- CIE XYZ -> a PNG you can look at.
//
// The last stage, and the only one whose output is not meant to be computed on.
// XYZ goes through the sRGB primaries to linear RGB, then through the sRGB
// transfer function -- the real piecewise one, with the linear segment near
// black, not a plain 2.2 gamma, which is wrong in the shadows.
//
// The clamp here is the sRGB gamut, not exposure. The colour matrix can carry a
// saturated sensor colour outside the display primaries even when the sensor
// itself never clipped, so a pixel can be clipped by the display encoding
// having survived the whole sensor chain intact.
//
// Nothing in this stage is tone mapping and nothing is auto-exposure. Brightness
// was fixed back at isp_blacklevel by the sensor's own window; if the picture
// is too dark, the exposure was too short, and the fix belongs in the sensor
// config rather than here.

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
                           "--in <xyz.exr> --out <srgb.png> [--linear]");
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> xyz[3];
  std::string error;
  if (!pipeline::ReadTriple(in, &width, &height, xyz, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  // --linear skips the transfer function. Truer to the numbers, but it buries
  // the shadows, so it is not the default.
  const bool gamma_encode = !args.Has("linear");

  std::vector<unsigned char> pixels;
  isp::XYZToSRGB8(xyz, gamma_encode, &pixels);

  if (!image_io::WritePNG8(out, width, height, 3, pixels, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, %s -> %s\n", args.program().c_str(), width, height,
              gamma_encode ? "sRGB transfer function" : "linear", out.c_str());
  return 0;
}
