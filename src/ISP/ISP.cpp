#include "ISP/ISP.hpp"

#include <algorithm>
#include <cmath>

#include "Spectrum.hpp"

namespace isp {

void ApplyBlackLevel(const std::vector<FP_PRECISION>& dn,
                     const SensorModel& sensor,
                     std::vector<FP_PRECISION>* normalised,
                     size_t* clipped_high, size_t* clipped_low) {
  const FP_PRECISION white = sensor.MaxDN();
  const FP_PRECISION black = sensor.BlackDN();

  size_t over = 0, under = 0;
  normalised->assign(dn.size(), 0.0);
  for (size_t i = 0; i < dn.size(); i++) {
    if (dn[i] >= white) over++;
    else if (dn[i] <= black) under++;
    (*normalised)[i] = sensor.NormalizedFromDN(dn[i]);
  }

  if (clipped_high) *clipped_high = over;
  if (clipped_low) *clipped_low = under;
}

void Demosaic(int width, int height, const std::vector<FP_PRECISION>& mosaic,
              const SensorModel& sensor, std::vector<FP_PRECISION> rgb[3]) {
  const size_t pixel_count = static_cast<size_t>(width) * height;
  for (int c = 0; c < 3; c++) rgb[c].assign(pixel_count, 0.0);

  for (int y = 0; y < height; y++) {
    for (int x = 0; x < width; x++) {
      const size_t index = static_cast<size_t>(y) * width + x;
      for (int c = 0; c < 3; c++) {
        const SensorChannel want = static_cast<SensorChannel>(c);
        if (sensor.ChannelAt(x, y) == want) {
          rgb[c][index] = mosaic[index];
          continue;
        }
        FP_PRECISION sum = 0.0;
        int count = 0;
        for (int dy = -1; dy <= 1; dy++) {
          for (int dx = -1; dx <= 1; dx++) {
            const int nx = x + dx, ny = y + dy;
            if (nx < 0 || nx >= width || ny < 0 || ny >= height) continue;
            if (sensor.ChannelAt(nx, ny) != want) continue;
            sum += mosaic[static_cast<size_t>(ny) * width + nx];
            count++;
          }
        }
        rgb[c][index] = count > 0 ? sum / count : 0.0;
      }
    }
  }
}

bool WhiteBalanceMethodByName(const std::string& name,
                              WhiteBalanceMethod* method) {
  if (name == "gains") { *method = WhiteBalanceMethod::kGains; return true; }
  if (name == "gray-world" || name == "grey-world") {
    *method = WhiteBalanceMethod::kGrayWorld;
    return true;
  }
  if (name == "max-rgb") { *method = WhiteBalanceMethod::kMaxRGB; return true; }
  return false;
}

void EstimateWhiteBalance(const std::vector<FP_PRECISION> rgb[3],
                          WhiteBalanceMethod method, FP_PRECISION gains[3]) {
  FP_PRECISION statistic[3] = {0, 0, 0};

  switch (method) {
    case WhiteBalanceMethod::kGrayWorld:
      for (int c = 0; c < 3; c++) {
        FP_PRECISION sum = 0.0;
        for (FP_PRECISION v : rgb[c]) sum += v;
        statistic[c] = rgb[c].empty() ? 0.0 : sum / rgb[c].size();
      }
      break;
    case WhiteBalanceMethod::kMaxRGB:
      for (int c = 0; c < 3; c++) {
        FP_PRECISION peak = 0.0;
        for (FP_PRECISION v : rgb[c]) peak = std::max(peak, v);
        statistic[c] = peak;
      }
      break;
    case WhiteBalanceMethod::kGains:
      return;  // caller supplied them
  }

  // Green is the reference: a Bayer CFA samples it twice as often, so it is the
  // least noisy channel and the one worth leaving untouched.
  for (int c = 0; c < 3; c++) {
    gains[c] = statistic[c] > 1e-30 ? statistic[1] / statistic[c] : 1.0;
  }
}

void ApplyWhiteBalance(std::vector<FP_PRECISION> rgb[3],
                       const FP_PRECISION gains[3]) {
  for (int c = 0; c < 3; c++) {
    for (FP_PRECISION& v : rgb[c]) v *= gains[c];
  }
}

void ApplyWhiteBalanceMap(std::vector<FP_PRECISION> rgb[3],
                          const std::vector<FP_PRECISION>& r_over_g,
                          const std::vector<FP_PRECISION>& b_over_g) {
  const size_t pixel_count = rgb[0].size();
  for (size_t i = 0; i < pixel_count; i++) {
    // A non-positive ratio means the map had nothing to say about this pixel.
    // Leaving the channel alone is the identity, which is safer than dividing
    // by something near zero and blowing the pixel out.
    if (r_over_g[i] > 1e-30) rgb[0][i] /= r_over_g[i];
    if (b_over_g[i] > 1e-30) rgb[2][i] /= b_over_g[i];
  }
}

void ApplyColorMatrix(const std::vector<FP_PRECISION> rgb[3],
                      const FP_PRECISION m[3][3],
                      std::vector<FP_PRECISION> xyz[3]) {
  const size_t pixel_count = rgb[0].size();
  for (int c = 0; c < 3; c++) xyz[c].assign(pixel_count, 0.0);

  for (size_t i = 0; i < pixel_count; i++) {
    const FP_PRECISION r = rgb[0][i], g = rgb[1][i], b = rgb[2][i];
    for (int row = 0; row < 3; row++) {
      xyz[row][i] = m[row][0] * r + m[row][1] * g + m[row][2] * b;
    }
  }
}

FP_PRECISION EncodeSRGB(FP_PRECISION v) {
  v = std::min(std::max(v, static_cast<FP_PRECISION>(0.0)),
               static_cast<FP_PRECISION>(1.0));
  return (v <= 0.0031308) ? (12.92 * v)
                          : (1.055 * std::pow(v, 1.0 / 2.4) - 0.055);
}

unsigned char Quantise8(FP_PRECISION unit) {
  return static_cast<unsigned char>(std::lround(
      std::min(std::max(unit, static_cast<FP_PRECISION>(0.0)),
               static_cast<FP_PRECISION>(1.0)) *
      255.0));
}

void XYZToSRGB8(const std::vector<FP_PRECISION> xyz[3], bool gamma_encode,
                std::vector<unsigned char>* out) {
  const size_t pixel_count = xyz[0].size();
  out->assign(pixel_count * 3, 0);
  for (size_t i = 0; i < pixel_count; i++) {
    const Vec3f linear = XYZToLinearSRGB(xyz[0][i], xyz[1][i], xyz[2][i]);
    const FP_PRECISION channel[3] = {linear.x, linear.y, linear.z};
    for (int c = 0; c < 3; c++) {
      (*out)[i * 3 + c] =
          Quantise8(gamma_encode ? EncodeSRGB(channel[c]) : channel[c]);
    }
  }
}

}  // namespace isp
