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

// tinyexr is configured to use stb's zlib, so both stb implementations have to
// be pulled in here for the decode/encode symbols. Same arrangement as
// src/BaseImage.cpp.
#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "../extern/stb_image_write.h"

#define TINYEXR_USE_MINIZ 0
#define TINYEXR_USE_STB_ZLIB 1
#define TINYEXR_IMPLEMENTATION
#include "../extern/tinyexr.h"

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
bool LoadImage(const std::string& path, Image& image) {
  if (!HasSuffix(path, ".exr")) {
    unsigned char* data =
        stbi_load(path.c_str(), &image.width, &image.height, nullptr, 3);
    if (!data) {
      std::fprintf(stderr, "imgdiff: cannot load %s\n", path.c_str());
      return false;
    }
    image.rgb.resize(image.PixelCount() * 3);
    for (size_t i = 0; i < image.rgb.size(); i++) {
      image.rgb[i] = static_cast<float>(data[i]);
    }
    stbi_image_free(data);
    return true;
  }

  float* out = nullptr;
  const char* err = nullptr;
  int ret = LoadEXR(&out, &image.width, &image.height, path.c_str(), &err);
  if (ret != TINYEXR_SUCCESS) {
    std::fprintf(stderr, "imgdiff: cannot load %s: %s\n", path.c_str(),
                 err ? err : "unknown error");
    if (err) FreeEXRErrorMessage(err);
    return false;
  }
  image.rgb.resize(image.PixelCount() * 3);
  for (size_t i = 0; i < image.PixelCount(); i++) {
    image.rgb[i * 3 + 0] = out[i * 4 + 0];
    image.rgb[i * 3 + 1] = out[i * 4 + 1];
    image.rgb[i * 3 + 2] = out[i * 4 + 2];
  }
  free(out);
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

  if (mode == "--compare" || mode == "--ratio" || mode == "--expect-ratio") {
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
