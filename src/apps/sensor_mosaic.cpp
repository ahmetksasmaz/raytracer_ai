// sensor_mosaic -- three channels -> the Bayer mosaic.
//
// A colour filter array puts one filter over each photosite, so a sensor
// measures one colour per pixel, not three. This stage does the throwing away:
// at each pixel it keeps whichever of the three electron counts the CFA
// actually would have recorded there, and discards the other two.
//
// Everything the ISP's demosaic later does is an attempt to guess back what is
// discarded here. Running this stage and isp_demosaic back to back on the same
// image is the cheapest way to see what that interpolation costs.

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
                           "--in <electrons.exr> --out <mosaic.exr> "
                           "--config <sensor.json> [--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> electrons[3];
  std::string error;
  if (!pipeline::ReadTriple(in, &width, &height, electrons, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::vector<FP_PRECISION> mosaic(static_cast<size_t>(width) * height, 0.0);
  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const size_t i = static_cast<size_t>(y) * width + x;
      mosaic[i] = electrons[static_cast<int>(sensor.ChannelAt(x, y))][i];
    }
  }

  if (!pipeline::WriteSingle(out, width, height, mosaic, "Y", &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, kept 1 of 3 channels per site -> %s\n",
              args.program().c_str(), width, height, out.c_str());
  return 0;
}
