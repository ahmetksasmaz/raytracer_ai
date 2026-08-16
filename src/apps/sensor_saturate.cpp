// sensor_saturate -- the full-well clamp.
//
// A photosite is a bucket with a finite capacity. Once it holds full_well
// electrons it cannot hold more, and every additional photon is lost. That is
// what clipping a highlight physically is.
//
// This happens in the well, BEFORE readout and before anything colour-related,
// and it happens per channel. That ordering is what makes a blown highlight
// shift hue rather than go neutral white: an over-exposed red site pegs while
// its green and blue neighbours are still climbing, so the ratio between the
// channels changes. Clamping after the colour matrix instead -- which is what a
// naive pipeline does -- would clip to white and lose that behaviour entirely.

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
                           "--in <noisy.exr> --out <wellclamped.exr> "
                           "--config <sensor.json> [--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> electrons;
  std::string error;
  if (!pipeline::ReadSingle(in, &width, &height, &electrons, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  size_t saturated = 0;
  for (FP_PRECISION& value : electrons) {
    if (value >= sensor.full_well_e) saturated++;
    value = sensor.Saturate(value);
  }

  if (!pipeline::WriteSingle(out, width, height, electrons, "Y", &error)) {
    return pipeline::Fail(args.program(), error);
  }

  // The saturated count is a measurement, not a warning. A camera pointed at
  // something too bright clips, and how much it clips is the result.
  const double total = static_cast<double>(electrons.size());
  std::printf("%s: %dx%d, full well %g e-, %.2f%% of sites saturated -> %s\n",
              args.program().c_str(), width, height,
              static_cast<double>(sensor.full_well_e),
              total > 0 ? 100.0 * saturated / total : 0.0, out.c_str());
  return 0;
}
