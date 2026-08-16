// sensor_ccm -- characterise a sensor's colour response.
//
// The odd one out: it takes no image. Both the white balance gains and the
// colour matrix depend only on the sensor's three spectral sensitivities and
// the illuminant, so this runs once per sensor and the result is reused for
// every frame that sensor captures.
//
// Both are fitted here together, and that is the point. A white balance and a
// colour matrix correct for the same thing -- that the sensor's channels are
// not the eye's -- so a matrix fitted without knowing whether the gains will be
// applied first will double-correct or under-correct the image. Emitting both
// from one fit means the ISP can name which pairing it is using and the two
// cannot drift apart.

#include <cstdio>

#include "Pipeline/Stage.hpp"
#include "Sensor/SensorCalibration.hpp"
#include "Sensor/SensorConfig.hpp"
#include "SpectrumLibrary.hpp"

int main(int argc, char** argv) {
  const pipeline::Arguments args(argc, argv);
  const std::string out = args.Get("out");
  const std::string config = args.Get("config");

  if (out.empty() || config.empty()) {
    return pipeline::Usage(args.program(),
                           "--config <sensor.json> --out <ccm.json> "
                           "[--illuminant D65|A|E|<library ref>] "
                           "[--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  // The illuminant the sensor is being characterised under. An unknown name is
  // fatal rather than a fallback to D65: quietly characterising against the
  // wrong light would put a colour cast into every frame this calibration
  // touches, and nothing downstream would show where it came from.
  const std::string illuminant_name = args.Get("illuminant", "D65");
  Spectrum illuminant;
  if (illuminant_name.find(':') != std::string::npos) {
    try {
      const SpectrumRecord& record =
          SpectrumLibrary::Instance().Require(illuminant_name);
      if (record.multichannel) {
        return pipeline::Fail(args.program(),
                              "'" + illuminant_name +
                                  "' is a camera sensitivity, not an "
                                  "illuminant");
      }
      illuminant = record.value;
    } catch (const std::exception& e) {
      return pipeline::Fail(args.program(), e.what());
    }
  } else if (!IlluminantByName(illuminant_name, illuminant)) {
    return pipeline::Fail(args.program(),
                          "unknown illuminant '" + illuminant_name +
                              "'. Known names: D65, A, E, or a 'light:' "
                              "library reference.");
  }

  const sensor_calibration::Calibration calibration =
      sensor_calibration::Fit(sensor, illuminant, illuminant_name);

  std::string error;
  if (!sensor_calibration::Write(out, calibration, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %s, gains %.4f %.4f %.4f, residual %.4f (after wb) "
              "%.4f (no wb) -> %s\n",
              args.program().c_str(), illuminant_name.c_str(),
              static_cast<double>(calibration.wb_gains[0]),
              static_cast<double>(calibration.wb_gains[1]),
              static_cast<double>(calibration.wb_gains[2]),
              static_cast<double>(calibration.residual_after_wb),
              static_cast<double>(calibration.residual_no_wb), out.c_str());
  if (!calibration.measured_training) {
    // Worth saying out loud: without the library the fit trains on uplifted
    // sRGB, which is smooth and easy for any sensor to match, so the residual
    // flatters the camera and describes the training data as much as the
    // sensor.
    std::printf("%s: trained on uplifted sRGB, not measured reflectance -- "
                "the residual describes the training data as much as the "
                "sensor. Put spectra/ on the search path.\n",
                args.program().c_str());
  }
  return 0;
}
