// raw_preview -- make a RAW openable, without correcting it.
//
// The RAW that sensor_adc writes is a single channel of digital numbers. No
// viewer can show that as tinted, because there is nothing in the file saying
// which pixel carried which filter -- so a RAW opened directly looks like a
// grey image with a faint checkerboard, and its actual colour behaviour is
// invisible.
//
// This writes it as 8-bit RGB, still in sensor space: NO white balance and NO
// colour matrix. The CFA's own channel gains survive, so a neutral subject
// comes out green. That cast is the point of this output -- it is what the
// sensor genuinely recorded, and it is what the ISP removes.
//
//   --mosaic   keep the Bayer sampling: each digital number is written only
//              into the channel its filter carries and the other two stay at
//              zero. Nothing is interpolated in to hide the sampling.
//              Beware previewing this scaled down: a nearest-neighbour resize
//              lands on one Bayer parity and the whole image takes that
//              channel's colour. An all-red "RGGB" thumbnail is the viewer, not
//              the file.
//   --linear   skip the sRGB transfer function.
//
// Shares isp_blacklevel's fixed window, so it is directly comparable with the
// final sRGB output rather than being exposed against its own content.

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
                           "--in <raw.pgm|raw.exr> --out <preview.png> "
                           "--config <sensor.json> [--mosaic] [--linear] "
                           "[--spectra <dir>]");
  }

  SensorModel sensor;
  try {
    sensor = sensor_config::Load(config, args.Get("spectra"));
  } catch (const std::exception& e) {
    return pipeline::Fail(args.program(), e.what());
  }

  int width = 0, height = 0;
  std::vector<FP_PRECISION> dn;
  std::string error;
  const bool is_pgm = in.size() > 4 && in.compare(in.size() - 4, 4, ".pgm") == 0;
  if (is_pgm) {
    int max_value = 0;
    if (!image_io::ReadPGM16(in, &width, &height, &dn, &max_value, &error)) {
      return pipeline::Fail(args.program(), error);
    }
  } else if (!pipeline::ReadSingle(in, &width, &height, &dn, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  const bool mosaic = args.Has("mosaic");
  const bool gamma_encode = !args.Has("linear");

  auto encode = [&](FP_PRECISION digital_number) {
    const FP_PRECISION unit = sensor.NormalizedFromDN(digital_number);
    return isp::Quantise8(gamma_encode ? isp::EncodeSRGB(unit) : unit);
  };

  const size_t pixel_count = static_cast<size_t>(width) * height;
  std::vector<unsigned char> pixels(pixel_count * 3, 0);

  if (mosaic) {
    for (int y = 0; y < height; y++) {
      for (int x = 0; x < width; x++) {
        const size_t i = static_cast<size_t>(y) * width + x;
        pixels[i * 3 + static_cast<int>(sensor.ChannelAt(x, y))] = encode(dn[i]);
      }
    }
  } else {
    std::vector<FP_PRECISION> rgb[3];
    isp::Demosaic(width, height, dn, sensor, rgb);
    for (size_t i = 0; i < pixel_count; i++) {
      for (int c = 0; c < 3; c++) pixels[i * 3 + c] = encode(rgb[c][i]);
    }
  }

  if (!image_io::WritePNG8(out, width, height, 3, pixels, &error)) {
    return pipeline::Fail(args.program(), error);
  }

  std::printf("%s: %dx%d, %s, %s, no white balance or colour matrix -> %s\n",
              args.program().c_str(), width, height,
              mosaic ? "mosaic" : "demosaiced",
              gamma_encode ? "gamma" : "linear", out.c_str());
  return 0;
}
