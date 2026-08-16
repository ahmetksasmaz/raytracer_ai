// isp_colormatrix -- sensorRGB -> CIE XYZ.
//
// The step that leaves the camera behind. Up to here every number has been
// "what this sensor's filters recorded", which is not comparable between two
// cameras. A 3x3 fitted against the standard observer moves it into XYZ, where
// it is comparable with anything.
//
// It is never exactly right. The sensor's three sensitivities would have to be
// a linear transform of the CIE matching functions -- the Luther condition --
// for a 3x3 to be exact, and real filters are not. The residual in the
// calibration file quantifies the error, and it is part of the result: it is
// how far this camera is from being colorimetric, and no amount of processing
// downstream recovers it.
//
// --matrix names which fit to use, and it must agree with whether
// isp_whitebalance ran. Using after_wb on unbalanced data under-corrects;
// using no_wb on balanced data corrects twice. The two produce plausible
// images either way, which is what makes getting it wrong easy to miss.

#include <cstdio>

#include "ISP/ISP.hpp"
#include "Pipeline/Stage.hpp"
#include "Sensor/SensorCalibration.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");
  const std::string calibration_path = args.Get("calibration");

  if (in.empty() || out.empty() || calibration_path.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <wb.exr> --out <xyz.exr> "
                           "--calibration <ccm.json> "
                           "[--matrix after_wb|no_wb]");
  }

  sensor_calibration::Calibration calibration;
  std::string error;
  if (!sensor_calibration::Read(calibration_path, &calibration, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const std::string which = args.Get("matrix", "after_wb");
  const FP_PRECISION (*m)[3];
  FP_PRECISION residual = 0.0;
  if (which == "after_wb") {
    m = calibration.matrix_after_wb;
    residual = calibration.residual_after_wb;
  } else if (which == "no_wb") {
    m = calibration.matrix_no_wb;
    residual = calibration.residual_no_wb;
  } else {
    return pipeline::Fail(args.program(),
                          "unknown matrix '" + which +
                              "'. Known: after_wb (the default, for data that "
                              "went through isp_whitebalance), no_wb.");
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> rgb[3];
  if (!pipeline::ReadTriple(in, &width, &height, rgb, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::vector<FP_PRECISION> xyz[3];
  isp::ApplyColorMatrix(rgb, m, xyz);

  if (!pipeline::WriteTriple(out, width, height, xyz, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, %s matrix for %s, residual %.4f -> %s\n",
              args.program().c_str(), width, height, which.c_str(),
              calibration.illuminant.c_str(), static_cast<double>(residual),
              out.c_str());
  return 0;
}
