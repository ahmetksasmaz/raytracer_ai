// isp_demosaic -- the mosaic -> three full-resolution channels.
//
// Undoing sensor_mosaic, except it cannot be undone: two thirds of the data was
// never measured. Every demosaic is an interpolation, and the difference
// between a good one and a bad one is where it puts the error -- false colour
// on fine detail, zippering on edges, or softness everywhere.
//
// The method here is the simple one: take the pixel's own value where the CFA
// carries that colour, average the neighbours that do otherwise. It is a
// starting point, not a demosaic study; a better interpolation replaces exactly
// one function in ISP.cpp.
//
// Still sensor space afterwards. The channels are what THIS camera's filters
// recorded, not red, green and blue in any standard sense, and comparing them
// across two sensors is meaningless until isp_colormatrix has run.

#include <cstdio>

#include "ISP/ISP.hpp"
#include "Pipeline/Stage.hpp"
#include "Sensor/SensorConfig.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");
  const std::string config = args.Get("config");

  if (in.empty() || out.empty() || config.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <linearized.exr> --out <demosaiced.exr> "
                           "--config <sensor.json> [--method bilinear] "
                           "[--spectra <dir>]");
  }

  const std::string method = args.Get("method", "bilinear");
  if (method != "bilinear") {
    return pipeline::Fail(args.program(),
                          "unknown demosaic method '" + method +
                              "'. Known: bilinear.");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> mosaic;
  std::string error;
  if (!pipeline::ReadSingle(in, &width, &height, &mosaic, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::vector<FP_PRECISION> rgb[3];
  isp::Demosaic(width, height, mosaic, sensor, rgb);

  if (!pipeline::WriteTriple(out, width, height, rgb, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, %s, still sensor space -> %s\n",
              args.program().c_str(), width, height, method.c_str(),
              out.c_str());
  return 0;
}
