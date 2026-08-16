// isp_whitebalance -- per-channel gains, so a neutral subject reads neutral.
//
// A RAW has a strong colour cast. Partly the light (an incandescent bulb really
// does emit far more red than blue), partly the sensor (a Bayer CFA has twice
// as many green sites and green filters that pass more light), so a grey card
// under tungsten might come off the sensor at roughly G/R 1.8, G/B 1.4. White
// balance is the correction: three gains that put the channels back level.
//
// Where the gains come from is the whole question:
//
//   --calibration <ccm.json>   the gains sensor_ccm computed from the sensor's
//                              own curves and a named illuminant. Correct by
//                              construction -- it knows both the light and the
//                              filters -- and the default.
//   --method gray-world        assume the scene averages to neutral. Wrong on
//                              any scene dominated by one colour.
//   --method max-rgb           assume the brightest reading in each channel is
//                              a white surface. Wrong as soon as a highlight
//                              clips, which pins all three channels together
//                              and reports no cast at all.
//   --gains "r g b"            supplied explicitly.
//
// The estimators are here because guessing the illuminant from the image is
// what a real camera has to do, and seeing them fail on a scene where the
// calibrated answer is known is the point of having both.
//
// Whichever is used, isp_colormatrix must then be given the matching matrix:
// --matrix after_wb if this stage ran, no_wb if it did not.

#include <cstdio>
#include <sstream>

#include "ISP/ISP.hpp"
#include "Pipeline/Stage.hpp"
#include "Sensor/SensorCalibration.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");

  if (in.empty() || out.empty()) {
    return pipeline::Usage(
        args.program(),
        "--in <demosaiced.exr> --out <wb.exr> "
        "[--calibration <ccm.json>] [--method gray-world|max-rgb] "
        "[--gains \"r g b\"]");
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> rgb[3];
  std::string error;
  if (!pipeline::ReadTriple(in, &width, &height, rgb, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  FP_PRECISION gains[3] = {1.0, 1.0, 1.0};
  std::string source;

  if (args.Has("gains")) {
    std::istringstream stream(args.Get("gains"));
    if (!(stream >> gains[0] >> gains[1] >> gains[2])) {
      return pipeline::Fail(args.program(),
                            "--gains expects three numbers, e.g. "
                            "--gains \"1.8 1.0 1.4\"");
    }
    source = "explicit";
  } else if (args.Has("method")) {
    const std::string name = args.Get("method");
    isp::WhiteBalanceMethod method;
    if (!isp::WhiteBalanceMethodByName(name, &method)) {
      return pipeline::Fail(args.program(),
                            "unknown white balance method '" + name +
                                "'. Known: gray-world, max-rgb.");
    }
    isp::EstimateWhiteBalance(rgb, method, gains);
    source = name;
  } else if (args.Has("calibration")) {
    sensor_calibration::Calibration calibration;
    if (!sensor_calibration::Read(args.Get("calibration"), &calibration,
                                  &error)) {
      return pipeline::Fail(args.program(), error);
    }
    for (int c = 0; c < 3; c++) gains[c] = calibration.wb_gains[c];
    source = "calibrated for " + calibration.illuminant;
  } else {
    return pipeline::Fail(args.program(),
                          "no gains given. Pass --calibration <ccm.json> for "
                          "the calibrated gains, --method for an estimate, or "
                          "--gains \"r g b\".");
  }

  isp::ApplyWhiteBalance(rgb, gains);

  if (!pipeline::WriteTriple(out, width, height, rgb, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, gains %.4f %.4f %.4f (%s) -> %s\n",
              args.program().c_str(), width, height,
              static_cast<double>(gains[0]), static_cast<double>(gains[1]),
              static_cast<double>(gains[2]), source.c_str(), out.c_str());
  std::printf("%s: pass --matrix after_wb to isp_colormatrix, or the image "
              "will be corrected twice\n", args.program().c_str());
  return 0;
}
