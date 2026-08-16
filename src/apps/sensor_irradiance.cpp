// sensor_irradiance -- spectral radiance at the sensor plane -> photons.
//
// The first thing that happens to light inside a camera, and the only purely
// radiometric step: how much of the scene's radiance the optics deliver to one
// pixel, and how many photons that energy amounts to.
//
//   photons(lambda) = L(lambda) * A * Omega * t * lambda/hc * bandwidth
//
// A is the pixel area, Omega the solid angle the aperture subtends (pi/4N^2 for
// f-number N), t the exposure time. The lambda/hc factor converts energy to a
// photon count and is wavelength dependent -- which is the whole reason this
// pipeline carries spectra rather than RGB triples. There is no way to do this
// step correctly on three numbers.
//
// Still colour-blind at this point: no filter, no quantum efficiency. Only the
// aperture, the pixel size and the shutter have been applied.

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
                           "--in <radiance.exr> --out <photons.exr> "
                           "--config <sensor.json> [--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<Spectrum> radiance;
  std::string error;
  if (!pipeline::ReadSpectral(in, &width, &height, &radiance, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::vector<Spectrum> photons(radiance.size());
  for (size_t i = 0; i < radiance.size(); i++) {
    photons[i] = sensor.PhotonsFromRadiance(radiance[i]);
  }

  if (!pipeline::WriteSpectral(out, width, height, photons, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, %d bands, exposure %g s, f/%g, pitch %g m -> %s\n",
              args.program().c_str(), width, height, kSpectralBands,
              static_cast<double>(sensor.exposure_time_s),
              static_cast<double>(sensor.f_number),
              static_cast<double>(sensor.pixel_pitch_m), out.c_str());
  return 0;
}
