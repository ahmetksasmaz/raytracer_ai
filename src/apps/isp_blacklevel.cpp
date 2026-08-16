// isp_blacklevel -- digital numbers -> linear [0,1].
//
// The first thing an ISP does: decide what counts as black and what counts as
// white, and put the RAW's integers on a scale where 0 is one and 1 is the
// other.
//
// The window is FIXED and comes from the sensor, not the frame. Saturation is
// the top of the ADC range and black is `DynamicRange` stops below it -- both
// constants of the hardware. Nothing here looks at the image histogram, which
// is deliberate: a scene-adaptive mapping would silently rescale a badly
// exposed sensor into looking correct, and hiding that is the opposite of what
// this pipeline is for. Exposure is something you set on the sensor until the
// histogram fits, and the clipped counts printed below are how you tell.
//
// Note the black point depends on full well and read noise, not just bit depth:
// DynamicRange defaults to log2(full_well / read_noise), the sensor's own
// engineering dynamic range. So this stage needs the whole sensor config.

#include <cstdio>

#include "ISP/ISP.hpp"
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
                           "--in <raw.pgm|raw.exr> --out <linearized.exr> "
                           "--config <sensor.json> [--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  // Reads either form of the RAW. The PGM is the authoritative one -- genuine
  // integers -- so it is preferred when the caller names it.
  int width = 0, height = 0;
  std::vector<FP_PRECISION> dn;
  std::string error;
  const bool is_pgm = in.size() > 4 && in.compare(in.size() - 4, 4, ".pgm") == 0;
  if (is_pgm) {
    int max_value = 0;
    if (!image_io::ReadPGM16(in, &width, &height, &dn, &max_value, &error)) {
      return pipeline::Fail(args.program(), error);
    }
    if (max_value != static_cast<int>(sensor.MaxDN())) {
      // A mismatch means the RAW and the config describe different sensors, and
      // every number downstream would be scaled wrong. Better to stop than to
      // produce a plausible image from mismatched parts.
      return pipeline::Fail(
          args.program(),
          in + " has a maximum value of " + std::to_string(max_value) +
              " but the config describes a " +
              std::to_string(sensor.bit_depth) + "-bit sensor (max " +
              std::to_string(static_cast<int>(sensor.MaxDN())) +
              "). The RAW and the config are for different sensors.");
    }
  } else if (!pipeline::ReadSingle(in, &width, &height, &dn, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::vector<FP_PRECISION> normalised;
  size_t over = 0, under = 0;
  isp::ApplyBlackLevel(dn, sensor, &normalised, &over, &under);

  if (!pipeline::WriteSingle(out, width, height, normalised, "Y", &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const double total = static_cast<double>(dn.size());
  std::printf("%s: %.1f stops, saturation at %.0f DN, black at %.2f DN"
              " -- %.2f%% clipped, %.2f%% below black -> %s\n",
              args.program().c_str(),
              static_cast<double>(sensor.DynamicRangeStops()),
              static_cast<double>(sensor.MaxDN()),
              static_cast<double>(sensor.BlackDN()),
              total > 0 ? 100.0 * over / total : 0.0,
              total > 0 ? 100.0 * under / total : 0.0, out.c_str());
  return 0;
}
