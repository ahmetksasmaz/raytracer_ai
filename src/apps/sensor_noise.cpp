// sensor_noise -- the three noise sources, in the order the hardware applies
// them.
//
//   shot noise    Poisson on the signal itself. Not a defect of the sensor:
//                 photon arrival is Poisson, so even a perfect detector counting
//                 a steady source sees sqrt(N) fluctuation. It is the reason a
//                 dark frame is noisy and a bright one is not.
//   dark current  Poisson, proportional to exposure time. Electrons that appear
//                 with no light at all, which is why a long exposure is noisy
//                 even with the lens capped.
//   read noise    Gaussian, added once at readout, independent of exposure.
//                 The noise floor a short exposure cannot get below.
//
// The seed is explicit and mandatory-by-default because this stage is a program
// now: the same input twice must give the same RAW twice, or a frame cannot be
// reproduced once you have looked at it. The renderer's thread-local generator
// would have made every run differ.

#include <cstdio>

#include "Core/Noise.hpp"
#include "Pipeline/Stage.hpp"
#include "Sensor/SensorConfig.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");
  const std::string config = args.Get("config");

  if (in.empty() || out.empty() || config.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <mosaic.exr> --out <noisy.exr> "
                           "--config <sensor.json> [--seed N] [--spectra <dir>]");
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

  const int seed = args.Integer("seed", 1);
  core::RandomGenerator rng(static_cast<uint64_t>(seed));

  for (FP_PRECISION& value : electrons) {
    value = sensor.ApplyNoise(value, rng);
  }

  if (!pipeline::WriteSingle(out, width, height, electrons, "Y", &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, sources%s%s%s, seed %d -> %s\n",
              args.program().c_str(), width, height,
              sensor.noise.shot_noise ? " shot" : "",
              sensor.noise.dark_current ? " dark" : "",
              sensor.noise.read_noise ? " read" : "",
              seed, out.c_str());
  if (!sensor.noise.shot_noise && !sensor.noise.dark_current &&
      !sensor.noise.read_noise) {
    std::printf("%s: all noise sources disabled, image passed through "
                "unchanged\n", args.program().c_str());
  }
  return 0;
}
