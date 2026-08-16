// sensor_cfa -- photons -> electrons, through the colour filters.
//
// Where the sensor stops being colour-blind. Each channel's total sensitivity
// is its colour filter transmittance times the quantum efficiency, and the
// electron count is the photon spectrum integrated against it. This is the step
// that collapses 31 numbers into 3 and throws away the spectrum: everything
// downstream is stuck with whatever these three curves preserved, which is
// exactly why two spectra that differ can end up identical here.
//
// All three channels are computed for every pixel, even though a Bayer sensor
// only records one of them per site. Keeping the full three-channel image makes
// this stage's output a useful reference -- it is what a three-layer sensor
// would have measured -- and sensor_mosaic is what discards two thirds of it.

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
                           "--in <photons.exr> --out <electrons.exr> "
                           "--config <sensor.json> [--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<Spectrum> photons;
  std::string error;
  if (!pipeline::ReadSpectral(in, &width, &height, &photons, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::vector<FP_PRECISION> electrons[3];
  for (int c = 0; c < 3; c++) electrons[c].assign(photons.size(), 0.0);

  for (size_t i = 0; i < photons.size(); i++) {
    for (int c = 0; c < 3; c++) {
      electrons[c][i] =
          sensor.ElectronsFromPhotons(photons[i], static_cast<SensorChannel>(c));
    }
  }

  if (!pipeline::WriteTriple(out, width, height, electrons, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, spectrum integrated against 3 channel "
              "sensitivities -> %s\n",
              args.program().c_str(), width, height, out.c_str());
  return 0;
}
