// sensor_adc -- gain and quantisation. The last thing the sensor does.
//
// Electrons are divided by the conversion gain to give a digital number, and
// the result is truncated to an integer in [0, 2^bits - 1]. After this point
// the measurement is discrete and nothing downstream can recover what fell
// between two codes.
//
// The floor is deliberate rather than a rounding choice: an ADC reports the
// code below the level it measured. The clamp at zero matters too -- read noise
// can drive a dark pixel below zero electrons, but no real converter returns a
// negative code.
//
// Writes the RAW twice. The PGM is the real artifact: 16-bit integers, a format
// with no interpretation attached, readable by anything. The EXR is the same
// numbers as floats, for tools in this pipeline that already speak EXR.
// Note both are SINGLE-channel -- a scalar mosaic, which no viewer can show as
// tinted. Use raw_preview to actually look at it.

#include <cstdio>

#include "ImageIO/ImageIO.hpp"
#include "Pipeline/Stage.hpp"
#include "Sensor/SensorConfig.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string in = args.Get("in");
  const std::string out = args.Get("out");
  const std::string config = args.Get("config");

  if (in.empty() || out.empty() || config.empty()) {
    return pipeline::Usage(args.program(),
                           "--in <wellclamped.exr> --out <raw.pgm> "
                           "--config <sensor.json> [--exr <raw.exr>] "
                           "[--spectra <dir>]");
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

  std::vector<FP_PRECISION> dn(electrons.size());
  for (size_t i = 0; i < electrons.size(); i++) {
    dn[i] = sensor.Quantise(electrons[i]);
  }

  char comment[160];
  std::snprintf(comment, sizeof(comment),
                "RAW Bayer mosaic in sensorRGB, %d-bit range, gain %g e-/DN",
                sensor.bit_depth, static_cast<double>(sensor.gain_e_per_dn));
  if (!image_io::WritePGM16(out, width, height, dn, sensor.bit_depth, comment,
                            &error)) {
    return pipeline::Fail(args.program(), error);
  }

  // The float EXR alongside it defaults to the PGM's name with the extension
  // swapped, so the pair stays together without the caller naming both.
  std::string exr_path = args.Get("exr");
  if (exr_path.empty()) {
    const size_t dot = out.find_last_of('.');
    exr_path = (dot == std::string::npos ? out : out.substr(0, dot)) + ".exr";
  }
  if (!pipeline::WriteSingle(exr_path, width, height, dn, "Y", &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, %d-bit, saturation at %.0f DN -> %s, %s\n",
              args.program().c_str(), width, height, sensor.bit_depth,
              static_cast<double>(sensor.MaxDN()), out.c_str(),
              exr_path.c_str());
  return 0;
}
