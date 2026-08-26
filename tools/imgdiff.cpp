// imgdiff — EXR assertion tool for the raytracer test suite.
//
// Uses the vendored tinyexr so the test harness needs no external dependencies.
// Every mode prints a human-readable summary and returns 0 on pass, 1 on fail,
// 2 on usage/IO error.
//
//   imgdiff --stats a.exr
//   imgdiff --compare a.exr b.exr --tol 0.01
//   imgdiff --expect-constant a.exr 0.5 --tol 0.01
//   imgdiff --expect-nonnegative a.exr
//   imgdiff --expect-finite a.exr
//   imgdiff --ratio a.exr b.exr
//   imgdiff --mean a.exr

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

// Decoding goes through rt_imageio, the one library that instantiates tinyexr
// and stb. imgdiff used to carry its own copy of both, which is why it had to
// be built as a separate program with its own compile rule.
#include "ImageIO/ImageIO.hpp"

namespace {

struct Image {
  int width = 0;
  int height = 0;
  std::vector<float> rgb;  // width * height * 3

  size_t PixelCount() const { return static_cast<size_t>(width) * height; }
};

bool HasSuffix(const std::string& s, const std::string& suffix) {
  return s.size() >= suffix.size() &&
         s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// EXR values are linear radiance; LDR values are kept in 0..255 so that
// assertions on tone-mapped output read in the units the file actually stores.
//
// Any channel layout loads. A 3-channel EXR maps to RGB by name; a 1-channel
// one (the RAW mosaic) is broadcast across all three, which is what the Bayer
// and variance modes expect; and an N-band spectral cube is averaged into all
// three, so --compare and --expect-finite work on a cube instead of failing
// with "R channel not found". Per-band assertions belong in --expect-channels,
// which reads the names.
bool LoadImage(const std::string& path, Image& image) {
  std::string error;

  if (!HasSuffix(path, ".exr")) {
    int channels = 3;
    std::vector<unsigned char> data;
    if (!image_io::ReadLDR(path, &image.width, &image.height, &channels, &data,
                           &error)) {
      std::fprintf(stderr, "imgdiff: %s\n", error.c_str());
      return false;
    }
    image.rgb.resize(data.size());
    for (size_t i = 0; i < data.size(); i++) {
      image.rgb[i] = static_cast<float>(data[i]);
    }
    return true;
  }

  image_io::ImagePlanes planes;
  if (!image_io::ReadMultiChannelEXR(path, &planes, &error)) {
    std::fprintf(stderr, "imgdiff: %s\n", error.c_str());
    return false;
  }

  image.width = planes.width;
  image.height = planes.height;
  const size_t pixel_count = image.PixelCount();
  image.rgb.assign(pixel_count * 3, 0.0f);

  const int r = planes.IndexOf("R");
  const int g = planes.IndexOf("G");
  const int b = planes.IndexOf("B");

  if (r >= 0 && g >= 0 && b >= 0) {
    for (size_t i = 0; i < pixel_count; i++) {
      image.rgb[i * 3 + 0] = planes.planes[r][i];
      image.rgb[i * 3 + 1] = planes.planes[g][i];
      image.rgb[i * 3 + 2] = planes.planes[b][i];
    }
  } else if (planes.ChannelCount() == 1) {
    for (size_t i = 0; i < pixel_count; i++) {
      const float v = planes.planes[0][i];
      image.rgb[i * 3 + 0] = image.rgb[i * 3 + 1] = image.rgb[i * 3 + 2] = v;
    }
  } else if (planes.ChannelCount() == 2) {
    // A chromaticity map. Averaging the two would make a file and the same file
    // with r/g and b/g swapped compare identical, so keep them apart.
    //
    // Resolved by NAME where the names are known. EXR stores channels sorted,
    // so b_over_g comes first in the file, and reading positionally prints the
    // two the wrong way round under headings that say "r=" and "g=" -- which is
    // exactly how the author of this branch misread his own output.
    const int rg = planes.IndexOf("r_over_g");
    const int bg = planes.IndexOf("b_over_g");
    const int first = rg >= 0 ? rg : 0;
    const int second = bg >= 0 ? bg : 1;
    for (size_t i = 0; i < pixel_count; i++) {
      image.rgb[i * 3 + 0] = planes.planes[first][i];
      image.rgb[i * 3 + 1] = planes.planes[second][i];
    }
  } else if (planes.ChannelCount() > 0) {
    const float inverse = 1.0f / planes.ChannelCount();
    for (size_t i = 0; i < pixel_count; i++) {
      float sum = 0.0f;
      for (const auto& plane : planes.planes) sum += plane[i];
      const float mean = sum * inverse;
      image.rgb[i * 3 + 0] = image.rgb[i * 3 + 1] = image.rgb[i * 3 + 2] = mean;
    }
  } else {
    std::fprintf(stderr, "imgdiff: %s has no channels\n", path.c_str());
    return false;
  }
  return true;
}

bool SameSize(const Image& a, const Image& b) {
  if (a.width != b.width || a.height != b.height) {
    std::fprintf(stderr,
                 "imgdiff: size mismatch: %dx%d vs %dx%d\n",
                 a.width, a.height, b.width, b.height);
    return false;
  }
  return true;
}

// Mean of each channel plus the overall mean, ignoring non-finite samples.
struct Stats {
  double mean_r = 0, mean_g = 0, mean_b = 0;
  double mean = 0;
  double min_v = 0, max_v = 0;
  size_t non_finite = 0;
  size_t negative = 0;
};

Stats ComputeStats(const Image& image) {
  Stats s;
  double sum[3] = {0, 0, 0};
  size_t counted = 0;
  bool first = true;
  for (size_t i = 0; i < image.rgb.size(); i++) {
    const double v = image.rgb[i];
    if (!std::isfinite(v)) {
      s.non_finite++;
      continue;
    }
    if (v < 0.0) s.negative++;
    if (first) {
      s.min_v = s.max_v = v;
      first = false;
    } else {
      s.min_v = std::min(s.min_v, v);
      s.max_v = std::max(s.max_v, v);
    }
    sum[i % 3] += v;
    counted++;
  }
  const double px = static_cast<double>(image.PixelCount());
  if (px > 0) {
    s.mean_r = sum[0] / px;
    s.mean_g = sum[1] / px;
    s.mean_b = sum[2] / px;
  }
  if (counted > 0) s.mean = (sum[0] + sum[1] + sum[2]) / counted;
  return s;
}

void PrintStats(const std::string& label, const Image& image, const Stats& s) {
  std::printf("%s: %dx%d  mean=%.6f (r=%.6f g=%.6f b=%.6f)  min=%.6f max=%.6f",
              label.c_str(), image.width, image.height, s.mean, s.mean_r,
              s.mean_g, s.mean_b, s.min_v, s.max_v);
  if (s.non_finite) std::printf("  NON-FINITE=%zu", s.non_finite);
  if (s.negative) std::printf("  NEGATIVE=%zu", s.negative);
  std::printf("\n");
}

// Relative RMSE is scale-invariant, which matters because these scenes span
// several orders of magnitude in radiance.
struct DiffResult {
  double rmse = 0;
  double relative_rmse = 0;
  double max_abs = 0;
  size_t non_finite = 0;
};

DiffResult Diff(const Image& a, const Image& b) {
  DiffResult d;
  double sum_sq = 0;
  double sum_ref_sq = 0;
  for (size_t i = 0; i < a.rgb.size(); i++) {
    const double va = a.rgb[i];
    const double vb = b.rgb[i];
    if (!std::isfinite(va) || !std::isfinite(vb)) {
      d.non_finite++;
      continue;
    }
    const double delta = va - vb;
    sum_sq += delta * delta;
    sum_ref_sq += vb * vb;
    d.max_abs = std::max(d.max_abs, std::fabs(delta));
  }
  const double n = static_cast<double>(a.rgb.size());
  d.rmse = std::sqrt(sum_sq / n);
  const double ref_rms = std::sqrt(sum_ref_sq / n);
  d.relative_rmse = ref_rms > 1e-9 ? d.rmse / ref_rms : d.rmse;
  return d;
}

double ArgTol(int argc, char** argv, double fallback) {
  for (int i = 1; i < argc - 1; i++) {
    if (std::strcmp(argv[i], "--tol") == 0) return std::atof(argv[i + 1]);
  }
  return fallback;
}

int Usage() {
  std::fprintf(stderr,
               "usage:\n"
               "  imgdiff --stats a.exr\n"
               "  imgdiff --mean a.exr\n"
               "  imgdiff --compare a.exr b.exr [--tol T]\n"
               "  imgdiff --expect-constant a.exr L [--tol T]\n"
               "  imgdiff --expect-nonnegative a.exr\n"
               "  imgdiff --expect-below a.png V\n"
               "  imgdiff --expect-finite a.exr\n"
               "  imgdiff --ratio a.exr b.exr\n"
               "  imgdiff --expect-ratio a.exr b.exr R [--tol T]\n");
  return 2;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc < 3) return Usage();
  const std::string mode = argv[1];

  if (mode == "--expect-pair") {
    // Asserts a two-channel map is constant, with the two named values. Used
    // for a chromaticity map whose illuminant is known in closed form.
    if (argc < 5) return Usage();
    const double want_a = std::atof(argv[3]);  // r_over_g
    const double want_b = std::atof(argv[4]);  // b_over_g
    const double tol = ArgTol(argc, argv, 0.01);

    image_io::ImagePlanes planes;
    std::string error;
    if (!image_io::ReadMultiChannelEXR(argv[2], &planes, &error)) {
      std::fprintf(stderr, "imgdiff: %s\n", error.c_str());
      return 2;
    }
    if (planes.ChannelCount() != 2) {
      std::printf("FAIL: %s has %d channels, expected 2\n", argv[2],
                  planes.ChannelCount());
      return 1;
    }

    // By NAME, not by position: EXR stores channels sorted, so b_over_g comes
    // back before r_over_g and a positional read compares each against the
    // other's expectation.
    const char* names[2] = {"r_over_g", "b_over_g"};
    const double want[2] = {want_a, want_b};
    double worst[2] = {0.0, 0.0};
    for (int c = 0; c < 2; c++) {
      const int index = planes.IndexOf(names[c]);
      if (index < 0) {
        std::printf("FAIL: %s has no '%s' channel\n", argv[2], names[c]);
        return 1;
      }
      for (float v : planes.planes[index]) {
        worst[c] = std::max(worst[c], std::fabs(v - want[c]));
      }
    }
    std::printf("%s: %s worst deviation %.6g (want %.6g), %s worst %.6g"
                " (want %.6g), tol %.6g\n",
                argv[2], names[0], worst[0], want[0],
                names[1], worst[1], want[1], tol);
    if (worst[0] > tol || worst[1] > tol) {
      std::printf("FAIL: map is not constant at the expected chromaticity\n");
      return 1;
    }
    std::printf("PASS\n");
    return 0;
  }

  if (mode == "--expect-product") {
    // Asserts a * b == c per channel, matched by channel NAME.
    //
    // Deliberately not routed through LoadImage: that averages an N-band cube
    // into grey, and mean(a)*mean(b) is not mean(a*b), so an averaged
    // comparison would be testing a different claim than the one intended.
    if (argc < 5) return Usage();
    const double tol = ArgTol(argc, argv, 0.001);

    image_io::ImagePlanes a, b, c;
    std::string error;
    if (!image_io::ReadMultiChannelEXR(argv[2], &a, &error) ||
        !image_io::ReadMultiChannelEXR(argv[3], &b, &error) ||
        !image_io::ReadMultiChannelEXR(argv[4], &c, &error)) {
      std::fprintf(stderr, "imgdiff: %s\n", error.c_str());
      return 2;
    }
    if (a.width != b.width || a.width != c.width ||
        a.height != b.height || a.height != c.height) {
      std::printf("FAIL: dimensions differ\n");
      return 1;
    }

    double sq_error = 0.0, sq_expected = 0.0, worst = 0.0;
    size_t compared = 0;
    for (int ch = 0; ch < a.ChannelCount(); ch++) {
      const int bi = b.IndexOf(a.names[ch]);
      const int ci = c.IndexOf(a.names[ch]);
      if (bi < 0 || ci < 0) {
        std::printf("FAIL: channel %s missing from one of the inputs\n",
                    a.names[ch].c_str());
        return 1;
      }
      for (size_t i = 0; i < a.PixelCount(); i++) {
        const double product = static_cast<double>(a.planes[ch][i]) *
                               static_cast<double>(b.planes[bi][i]);
        const double actual = c.planes[ci][i];
        const double d = product - actual;
        sq_error += d * d;
        sq_expected += actual * actual;
        worst = std::max(worst, std::fabs(d));
        compared++;
      }
    }
    const double relative =
        sq_expected > 0 ? std::sqrt(sq_error / sq_expected) : 0.0;
    std::printf("%s x %s vs %s: %zu samples over %d channels,"
                " relative_rmse=%.6g max_abs=%.6g tol=%.6g\n",
                argv[2], argv[3], argv[4], compared, a.ChannelCount(),
                relative, worst, tol);
    if (relative > tol) {
      std::printf("FAIL: the product does not reproduce the third image\n");
      return 1;
    }
    std::printf("PASS\n");
    return 0;
  }

  if (mode == "--expect-channels") {
    // Verifies a multi-channel EXR (the spectral cube) is well formed: the
    // right number of channels, and names that parse as wavelengths in
    // ascending order.
    //
    // The names are the assertion that matters. They are the only record of
    // which band is which -- nothing else in the file says -- so a cube whose
    // channels are named wrongly would be read as the right count of the wrong
    // wavelengths, which no pixel comparison would catch.
    if (argc < 4) return Usage();
    const int want = std::atoi(argv[3]);

    image_io::ImagePlanes planes;
    std::string error;
    if (!image_io::ReadMultiChannelEXR(argv[2], &planes, &error)) {
      std::fprintf(stderr, "imgdiff: %s\n", error.c_str());
      return 2;
    }

    std::printf("%s: %d channels", argv[2], planes.ChannelCount());
    for (int i = 0; i < std::min(planes.ChannelCount(), 3); i++)
      std::printf(" %s", planes.names[i].c_str());
    if (planes.ChannelCount() > 3)
      std::printf(" ... %s", planes.names.back().c_str());
    std::printf("  (expected %d)\n", want);

    if (planes.ChannelCount() != want) {
      std::printf("FAIL: channel count mismatch\n");
      return 1;
    }

    std::vector<double> wavelengths;
    if (!image_io::ParseBandWavelengths(planes, &wavelengths)) {
      std::printf("FAIL: channels are not named by wavelength\n");
      return 1;
    }
    for (size_t i = 1; i < wavelengths.size(); i++) {
      if (!(wavelengths[i] > wavelengths[i - 1])) {
        std::printf("FAIL: wavelengths are not ascending at channel %zu"
                    " (%.0fnm after %.0fnm)\n",
                    i, wavelengths[i], wavelengths[i - 1]);
        return 1;
      }
    }
    std::printf("bands %.0fnm..%.0fnm, ascending\n", wavelengths.front(),
                wavelengths.back());
    std::printf("PASS\n");
    return 0;
  }

  Image a;
  if (!LoadImage(argv[2], a)) return 2;

  if (mode == "--stats") {
    PrintStats(argv[2], a, ComputeStats(a));
    return 0;
  }

  if (mode == "--mean") {
    std::printf("%.8f\n", ComputeStats(a).mean);
    return 0;
  }

  if (mode == "--expect-nonnegative") {
    const Stats s = ComputeStats(a);
    PrintStats(argv[2], a, s);
    if (s.negative > 0) {
      std::printf("FAIL: %zu negative samples (min %.6f)\n", s.negative, s.min_v);
      return 1;
    }
    std::printf("PASS: no negative samples\n");
    return 0;
  }

  if (mode == "--expect-channel-max") {
    // Asserts which colour channel dominates. Used to check that a spectral
    // reflectance actually produces the colour its spectrum implies.
    if (argc < 4) return Usage();
    const std::string want = argv[3];
    const Stats s = ComputeStats(a);
    PrintStats(argv[2], a, s);
    const double r = s.mean_r, g = s.mean_g, b = s.mean_b;
    const bool ok = (want == "r" && r > g && r > b) ||
                    (want == "g" && g > r && g > b) ||
                    (want == "b" && b > r && b > g);
    std::printf("channel means r=%.6f g=%.6f b=%.6f, expected '%s' largest\n", r,
                g, b, want.c_str());
    if (!ok) {
      std::printf("FAIL: '%s' is not the dominant channel\n", want.c_str());
      return 1;
    }
    std::printf("PASS\n");
    return 0;
  }

  if (mode == "--expect-bayer") {
    // Structural check on a single-channel Bayer mosaic. The two green sites of
    // the 2x2 tile carry the SAME filter, so their means must agree; the red and
    // blue sites carry different filters, so they must differ from green. This
    // validates the mosaic layout and the CFA together, and would fail if the
    // pattern were transposed or the filters were not actually being applied.
    if (argc < 4) return Usage();
    const std::string pattern = argv[3];
    double sum[2][2] = {{0, 0}, {0, 0}};
    long count[2][2] = {{0, 0}, {0, 0}};
    for (int y = 0; y < a.height; y++) {
      for (int x = 0; x < a.width; x++) {
        const double v = a.rgb[(static_cast<size_t>(y) * a.width + x) * 3];
        sum[y & 1][x & 1] += v;
        count[y & 1][x & 1]++;
      }
    }
    double m[2][2];
    for (int r = 0; r < 2; r++)
      for (int c = 0; c < 2; c++) m[r][c] = count[r][c] ? sum[r][c] / count[r][c] : 0.0;

    std::printf("%s: 2x2 parity means (0,0)=%.2f (1,0)=%.2f (0,1)=%.2f (1,1)=%.2f\n",
                argv[2], m[0][0], m[0][1], m[1][0], m[1][1]);

    // Locate the two green sites for the declared pattern.
    double g1, g2, other1, other2;
    if (pattern == "RGGB" || pattern == "BGGR") {
      g1 = m[0][1]; g2 = m[1][0]; other1 = m[0][0]; other2 = m[1][1];
    } else {  // GRBG, GBRG
      g1 = m[0][0]; g2 = m[1][1]; other1 = m[0][1]; other2 = m[1][0];
    }
    const double green_mean = 0.5 * (g1 + g2);
    const double green_mismatch =
        green_mean > 1e-9 ? std::fabs(g1 - g2) / green_mean : 0.0;
    std::printf("green sites %.2f vs %.2f (mismatch %.4f); other sites %.2f, %.2f\n",
                g1, g2, green_mismatch, other1, other2);

    if (green_mismatch > 0.02) {
      std::printf("FAIL: the two green sites disagree -- mosaic layout is wrong\n");
      return 1;
    }
    // The two green sites agreeing is the sharp structural test: it fails
    // immediately if the pattern is transposed or the parities are swapped.
    //
    // Requiring BOTH red and blue to differ from green would be too strict --
    // under a broadly flat illuminant a red filter can integrate to nearly the
    // same total as a green one. Requiring the largest separation to be real is
    // enough to prove the CFA is applied at all.
    const double sep1 = std::fabs(other1 - green_mean) / std::max(green_mean, 1e-9);
    const double sep2 = std::fabs(other2 - green_mean) / std::max(green_mean, 1e-9);
    std::printf("separation from green: %.4f and %.4f (max must exceed 0.05)\n",
                sep1, sep2);
    if (std::max(sep1, sep2) < 0.05) {
      std::printf("FAIL: red and blue both match green -- the CFA is not being applied\n");
      return 1;
    }
    std::printf("PASS\n");
    return 0;
  }

  if (mode == "--variance") {
    // Per-Bayer-parity mean and variance.
    //
    // Whole-image variance is useless on a mosaic: the R/G/B sites carry
    // different filters, so the mosaic pattern itself dominates and a
    // completely noise-free render still measures large "variance". Within one
    // parity class the noise-free signal is constant, so the variance there is
    // genuinely the noise.
    //
    // With gain g, Poisson electrons of mean E give DN of mean E/g and variance
    // E/g^2, so variance/mean in DN should come out at 1/g for a shot-noise
    // dominated exposure. That ratio is printed as the physical check.
    double sum[2][2] = {{0, 0}, {0, 0}};
    long count[2][2] = {{0, 0}, {0, 0}};
    for (int y = 0; y < a.height; y++)
      for (int x = 0; x < a.width; x++) {
        sum[y & 1][x & 1] += a.rgb[(static_cast<size_t>(y) * a.width + x) * 3];
        count[y & 1][x & 1]++;
      }
    double worst_var = 0.0;
    for (int r = 0; r < 2; r++) {
      for (int c = 0; c < 2; c++) {
        const double mean = count[r][c] ? sum[r][c] / count[r][c] : 0.0;
        double var = 0.0;
        for (int y = r; y < a.height; y += 2)
          for (int x = c; x < a.width; x += 2) {
            const double d = a.rgb[(static_cast<size_t>(y) * a.width + x) * 3] - mean;
            var += d * d;
          }
        var = count[r][c] > 1 ? var / (count[r][c] - 1) : 0.0;
        worst_var = std::max(worst_var, var);
        std::printf("parity(%d,%d) mean=%.3f variance=%.4f var/mean=%.5f\n", r, c,
                    mean, var, mean > 1e-9 ? var / mean : 0.0);
      }
    }

    // Optional assertions so this can drive a test directly.
    for (int i = 1; i < argc - 1; i++) {
      if (std::strcmp(argv[i], "--expect-noiseless") == 0) {
        if (worst_var > std::atof(argv[i + 1])) {
          std::printf("FAIL: variance %.6f exceeds noiseless limit %s\n",
                      worst_var, argv[i + 1]);
          return 1;
        }
        std::printf("PASS: noise-free within %s\n", argv[i + 1]);
      }
      if (std::strcmp(argv[i], "--expect-noisy") == 0) {
        if (worst_var < std::atof(argv[i + 1])) {
          std::printf("FAIL: variance %.6f below expected noise floor %s\n",
                      worst_var, argv[i + 1]);
          return 1;
        }
        std::printf("PASS: noise present (variance %.4f)\n", worst_var);
      }
    }
    return 0;
  }

  if (mode == "--argmax") {
    // Locates the brightest sample. Useful for tracking down single-pixel
    // fireflies, which a mean-based assertion reports but cannot place.
    size_t best = 0;
    double best_v = -1e300;
    for (size_t i = 0; i < a.rgb.size(); i++) {
      if (std::isfinite(a.rgb[i]) && a.rgb[i] > best_v) {
        best_v = a.rgb[i];
        best = i;
      }
    }
    const size_t pixel = best / 3;
    std::printf("max %.6f at pixel (x=%d, y=%d) channel %d of %dx%d\n", best_v,
                static_cast<int>(pixel % a.width),
                static_cast<int>(pixel / a.width), static_cast<int>(best % 3),
                a.width, a.height);
    return 0;
  }

  if (mode == "--expect-below") {
    if (argc < 4) return Usage();
    const double limit = std::atof(argv[3]);
    const Stats s = ComputeStats(a);
    PrintStats(argv[2], a, s);
    if (s.non_finite > 0) {
      std::printf("FAIL: %zu non-finite samples\n", s.non_finite);
      return 1;
    }
    if (s.max_v > limit) {
      std::printf("FAIL: max %.6f exceeds limit %.6f\n", s.max_v, limit);
      return 1;
    }
    std::printf("PASS: max %.6f within limit %.6f\n", s.max_v, limit);
    return 0;
  }

  if (mode == "--expect-finite") {
    const Stats s = ComputeStats(a);
    PrintStats(argv[2], a, s);
    if (s.non_finite > 0) {
      std::printf("FAIL: %zu non-finite samples\n", s.non_finite);
      return 1;
    }
    std::printf("PASS: all samples finite\n");
    return 0;
  }

  if (mode == "--expect-constant") {
    if (argc < 4) return Usage();
    const double expected = std::atof(argv[3]);
    const double tol = ArgTol(argc, argv, 0.01);
    const Stats s = ComputeStats(a);
    PrintStats(argv[2], a, s);

    // Compare the mean rather than every pixel: these renders are stochastic,
    // so per-pixel equality would just be a noise test. Max deviation is
    // reported for diagnosis but does not drive the verdict.
    const double err = std::fabs(s.mean - expected);
    const double rel = expected > 1e-9 ? err / expected : err;
    std::printf("expected=%.6f actual=%.6f abs_err=%.6f rel_err=%.4f tol=%.4f\n",
                expected, s.mean, err, rel, tol);
    if (s.non_finite > 0) {
      std::printf("FAIL: %zu non-finite samples\n", s.non_finite);
      return 1;
    }
    if (rel > tol) {
      std::printf("FAIL: relative error %.4f exceeds tolerance %.4f\n", rel, tol);
      return 1;
    }

    // Optional spread check. Some configurations are analytically noise-free
    // (cosine sampling in a uniform field evaluates to rho*L for EVERY sample),
    // and there a flat mean can still hide per-sample errors that happen to
    // cancel. --max-dev asserts the actual spread, not just the average.
    for (int i = 1; i < argc - 1; i++) {
      if (std::strcmp(argv[i], "--max-dev") != 0) continue;
      const double max_dev = std::atof(argv[i + 1]);
      const double worst =
          std::max(std::fabs(s.max_v - expected), std::fabs(s.min_v - expected));
      std::printf("worst per-pixel deviation %.6f (limit %.6f)\n", worst, max_dev);
      if (worst > max_dev) {
        std::printf("FAIL: per-pixel deviation %.6f exceeds %.6f\n", worst, max_dev);
        return 1;
      }
    }

    std::printf("PASS\n");
    return 0;
  }

  if (mode == "--compare" || mode == "--ratio" || mode == "--expect-ratio" || mode == "--expect-differ") {
    if (argc < 4) return Usage();
    Image b;
    if (!LoadImage(argv[3], b)) return 2;
    if (!SameSize(a, b)) return 2;

    const Stats sa = ComputeStats(a);
    const Stats sb = ComputeStats(b);
    PrintStats(argv[2], a, sa);
    PrintStats(argv[3], b, sb);

    if (mode == "--ratio") {
      const double ratio = sb.mean > 1e-12 ? sa.mean / sb.mean : 0.0;
      std::printf("ratio(mean a / mean b) = %.8f\n", ratio);
      return 0;
    }

    if (mode == "--expect-differ") {
      // The inverse of --compare: asserts two renders are meaningfully
      // DIFFERENT. Used where a feature is only doing its job if it changes the
      // image, e.g. rendering the same scene under two different illuminants.
      const double min_diff = ArgTol(argc, argv, 0.05);
      const double mean_rel =
          sb.mean > 1e-9 ? std::fabs(sa.mean - sb.mean) / sb.mean : 0.0;
      std::printf("mean_rel_diff=%.4f required>=%.4f\n", mean_rel, min_diff);
      if (mean_rel < min_diff) {
        std::printf(
            "FAIL: images are too similar (%.4f) -- the feature under test "
            "appears to have no effect\n",
            mean_rel);
        return 1;
      }
      std::printf("PASS\n");
      return 0;
    }

    if (mode == "--expect-ratio") {
      if (argc < 5) return Usage();
      const double expected = std::atof(argv[4]);
      const double tol = ArgTol(argc, argv, 0.02);
      const double ratio = sb.mean > 1e-12 ? sa.mean / sb.mean : 0.0;
      const double rel =
          expected > 1e-9 ? std::fabs(ratio - expected) / expected : ratio;
      std::printf("ratio=%.8f expected=%.8f rel_err=%.4f tol=%.4f\n", ratio,
                  expected, rel, tol);
      if (rel > tol) {
        std::printf("FAIL: ratio off by %.4f (tolerance %.4f)\n", rel, tol);
        return 1;
      }
      std::printf("PASS\n");
      return 0;
    }

    const double tol = ArgTol(argc, argv, 0.01);
    const DiffResult d = Diff(a, b);
    const double mean_rel =
        sb.mean > 1e-9 ? std::fabs(sa.mean - sb.mean) / sb.mean : 0.0;
    std::printf(
        "rmse=%.6f relative_rmse=%.4f max_abs=%.6f mean_rel_err=%.4f tol=%.4f\n",
        d.rmse, d.relative_rmse, d.max_abs, mean_rel, tol);
    if (d.non_finite > 0) {
      std::printf("FAIL: %zu non-finite samples\n", d.non_finite);
      return 1;
    }
    // Mean agreement is the meaningful criterion for two stochastic renders of
    // the same scene; per-pixel RMSE stays noisy no matter how correct the
    // estimator is.
    if (mean_rel > tol) {
      std::printf("FAIL: mean relative error %.4f exceeds tolerance %.4f\n",
                  mean_rel, tol);
      return 1;
    }
    std::printf("PASS\n");
    return 0;
  }

  return Usage();
}
