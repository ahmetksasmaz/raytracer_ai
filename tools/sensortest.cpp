// Self-checks for the sensor model.
//
// Noise is validated statistically rather than by eye. A wrong noise model
// still produces a grainy-looking image, so "it looks noisy" proves nothing;
// what distinguishes a correct one is that the moments come out right. Poisson
// shot noise in particular has variance EQUAL to its mean, which is a sharp,
// falsifiable prediction.

#include <cmath>
#include <cstdio>
#include <string>
#include <vector>

#include "../include/Sensor/SensorModel.hpp"

namespace {
// One generator for the whole run. The sensor stages take their generator as a
// parameter now, so the tests supply one explicitly too -- and a fixed seed
// makes a statistical failure reproducible rather than something that shows up
// once in ten runs.
core::RandomGenerator rng(20240816);
}  // namespace

namespace {

int failures = 0;

void Check(bool ok, const std::string& what, const std::string& detail) {
  std::printf("  %-46s %s   %s\n", what.c_str(), ok ? "PASS" : "FAIL",
              detail.c_str());
  if (!ok) failures++;
}

struct Moments {
  double mean = 0, variance = 0;
};

Moments SampleMoments(const std::vector<double>& xs) {
  Moments m;
  for (double x : xs) m.mean += x;
  m.mean /= xs.size();
  for (double x : xs) m.variance += (x - m.mean) * (x - m.mean);
  m.variance /= (xs.size() - 1);
  return m;
}

}  // namespace

int main() {
  std::printf("sensor model self-checks\n");

  // --- Poisson moments -----------------------------------------------------
  // Both regimes of SamplePoisson are exercised: the exact Knuth path below the
  // threshold and the Gaussian approximation above it. Variance must track the
  // mean in both, which is what makes this a real test of the distribution
  // rather than just of its average.
  for (double lambda : {2.0, 25.0, 500.0, 50000.0}) {
    std::vector<double> draws;
    draws.reserve(200000);
    for (int i = 0; i < 200000; i++) draws.push_back(rng.Poisson(lambda));
    const Moments m = SampleMoments(draws);
    const double mean_err = std::fabs(m.mean - lambda) / lambda;
    const double var_ratio = m.variance / lambda;
    char detail[200];
    std::snprintf(detail, sizeof(detail),
                  "lambda=%-7.0f mean=%.1f (err %.3f)  var/mean=%.3f", lambda,
                  m.mean, mean_err, var_ratio);
    Check(mean_err < 0.02 && std::fabs(var_ratio - 1.0) < 0.06,
          "Poisson mean and variance both equal lambda", detail);
  }

  // Poisson must never return a negative count, even where the Gaussian
  // approximation's tail would.
  {
    double most_negative = 0.0;
    for (int i = 0; i < 100000; i++)
      most_negative = std::min(most_negative, rng.Poisson(40.0));
    char detail[80];
    std::snprintf(detail, sizeof(detail), "min draw %.1f", most_negative);
    Check(most_negative >= 0.0, "Poisson draws are non-negative", detail);
  }

  // --- Gaussian read noise -------------------------------------------------
  {
    const double sigma = 3.0;
    std::vector<double> draws;
    draws.reserve(200000);
    for (int i = 0; i < 200000; i++) draws.push_back(rng.Gaussian(0.0, sigma));
    const Moments m = SampleMoments(draws);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "mean=%.4f (want 0)  sd=%.4f (want %.1f)",
                  m.mean, std::sqrt(m.variance), sigma);
    Check(std::fabs(m.mean) < 0.05 &&
              std::fabs(std::sqrt(m.variance) - sigma) / sigma < 0.02,
          "Gaussian read noise has the requested moments", detail);
  }

  // --- Bayer layout --------------------------------------------------------
  // Each pattern is checked at the four parities of its 2x2 tile.
  {
    struct Case {
      BayerPattern pattern;
      const char* name;
      SensorChannel expect[4];  // (0,0) (1,0) (0,1) (1,1)
    };
    const Case cases[] = {
        {BayerPattern::kRGGB, "RGGB",
         {SensorChannel::kRed, SensorChannel::kGreen, SensorChannel::kGreen,
          SensorChannel::kBlue}},
        {BayerPattern::kBGGR, "BGGR",
         {SensorChannel::kBlue, SensorChannel::kGreen, SensorChannel::kGreen,
          SensorChannel::kRed}},
        {BayerPattern::kGRBG, "GRBG",
         {SensorChannel::kGreen, SensorChannel::kRed, SensorChannel::kBlue,
          SensorChannel::kGreen}},
        {BayerPattern::kGBRG, "GBRG",
         {SensorChannel::kGreen, SensorChannel::kBlue, SensorChannel::kRed,
          SensorChannel::kGreen}},
    };
    bool all_ok = true;
    for (const Case& c : cases) {
      SensorModel s;
      s.pattern = c.pattern;
      all_ok &= s.ChannelAt(0, 0) == c.expect[0];
      all_ok &= s.ChannelAt(1, 0) == c.expect[1];
      all_ok &= s.ChannelAt(0, 1) == c.expect[2];
      all_ok &= s.ChannelAt(1, 1) == c.expect[3];
      // and that it tiles
      all_ok &= s.ChannelAt(2, 2) == c.expect[0];
      all_ok &= s.ChannelAt(7, 5) == c.expect[3];
    }
    Check(all_ok, "all four Bayer patterns tile correctly",
          "checked 2x2 parities plus tiling");
  }

  // --- The split matches the fused path ------------------------------------
  // The sensor is a chain of separate programs now, and the first two stages
  // together must reproduce exactly what the fused ElectronsAt computes:
  //
  //   ElectronsFromPhotons(PhotonsFromRadiance(L), ChannelAt(x, y))
  //       == ElectronsAt(L, x, y)
  //
  // This is the assertion the split needs and no physics check provides. A
  // factor dropped in the handover between two programs -- the band width, the
  // geometry factor A*Omega*t, the photon energy hc/lambda -- leaves each stage
  // looking individually plausible and only shows up in the composition. It is
  // checked over several spectra and both CFA parities so a mistake cannot hide
  // in one channel or in a flat spectrum's symmetry.
  {
    SensorModel s;
    DefaultBayerCFA(s.cfa);
    s.exposure_time_s = 2.5e-4;
    s.f_number = 4.0;
    s.pixel_pitch_m = 2.2e-6;

    Spectrum sloped, spike;
    for (int b = 0; b < kSpectralBands; b++) {
      sloped[b] = 0.1 + 0.9 * b / static_cast<double>(kSpectralBands - 1);
      spike[b] = (BandWavelength(b) > 540 && BandWavelength(b) < 560) ? 5.0 : 0.0;
    }
    const Spectrum cases[3] = {Spectrum::Constant(1.0), sloped, spike};

    double worst = 0.0;
    bool positive = false;
    for (const Spectrum& radiance : cases) {
      const Spectrum photons = s.PhotonsFromRadiance(radiance);
      for (int y = 0; y < 2; y++) {
        for (int x = 0; x < 2; x++) {
          const double fused = s.ElectronsAt(radiance, x, y);
          const double split =
              s.ElectronsFromPhotons(photons, s.ChannelAt(x, y));
          if (fused > 0.0) positive = true;
          const double relative =
              fused > 0.0 ? std::fabs(split - fused) / fused
                          : std::fabs(split - fused);
          worst = std::max(worst, relative);
        }
      }
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail),
                  "3 spectra x 4 parities, worst relative difference %.3e",
                  worst);
    Check(positive && worst < 1e-12,
          "split stages reproduce the fused electron count", detail);
  }

  // --- Radiometry ----------------------------------------------------------
  // Electron count must be exactly linear in radiance and in exposure time:
  // both are simple multiplicative factors in the photon integral, so any
  // departure means the chain is wrong.
  {
    SensorModel s;
    DefaultBayerCFA(s.cfa);
    const Spectrum unit = Spectrum::Constant(1.0);
    const double e1 = s.ElectronsAt(unit, 0, 0);
    const double e2 = s.ElectronsAt(Spectrum::Constant(2.0), 0, 0);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "e(L=1)=%.1f e(L=2)=%.1f ratio=%.6f",
                  e1, e2, e2 / e1);
    Check(e1 > 0 && std::fabs(e2 / e1 - 2.0) < 1e-9,
          "electrons are linear in radiance", detail);
  }
  {
    SensorModel s;
    DefaultBayerCFA(s.cfa);
    s.exposure_time_s = 0.01;
    const double e1 = s.ElectronsAt(Spectrum::Constant(1.0), 0, 0);
    s.exposure_time_s = 0.03;
    const double e3 = s.ElectronsAt(Spectrum::Constant(1.0), 0, 0);
    char detail[120];
    std::snprintf(detail, sizeof(detail), "ratio=%.6f (want 3)", e3 / e1);
    // PhotonsPerUnitRadiance caches per model, so this also checks the cache is
    // not shared across differently configured sensors.
    Check(std::fabs(e3 / e1 - 3.0) < 1e-6, "electrons are linear in exposure time",
          detail);
  }

  // The red filter must respond more to long wavelengths than the blue one.
  {
    SensorModel s;
    DefaultBayerCFA(s.cfa);
    Spectrum red_light;
    for (int b = 0; b < kSpectralBands; b++)
      if (BandWavelength(b) > 610) red_light[b] = 1.0;
    // (0,0) is red and (1,1) is blue under RGGB.
    const double on_red = s.ElectronsAt(red_light, 0, 0);
    const double on_blue = s.ElectronsAt(red_light, 1, 1);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "red pixel %.1f e-, blue pixel %.1f e-",
                  on_red, on_blue);
    Check(on_red > 10.0 * std::max(on_blue, 1e-9),
          "CFA separates channels (long-wave light -> red pixel)", detail);
  }

  // --- Quantisation --------------------------------------------------------
  {
    SensorModel s;
    s.noise.shot_noise = s.noise.read_noise = s.noise.dark_current = false;
    s.bit_depth = 12;
    s.gain_e_per_dn = 1.0;
    s.full_well_e = 1e9;  // isolate the ADC ceiling from well saturation
    const double huge = s.ElectronsToDN(1e8, rng);
    const double neg = s.ElectronsToDN(-50.0, rng);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "saturated=%.0f (max %.0f), negative=%.0f",
                  huge, s.MaxDN(), neg);
    Check(huge == s.MaxDN() && neg == 0.0,
          "DN clamps to [0, 2^bits - 1]", detail);
  }

  // Full-well saturation happens in the well, before the ADC sees it.
  {
    SensorModel s;
    s.noise.shot_noise = s.noise.read_noise = s.noise.dark_current = false;
    s.full_well_e = 1000.0;
    s.gain_e_per_dn = 1.0;
    s.bit_depth = 16;
    char detail[120];
    std::snprintf(detail, sizeof(detail), "DN=%.0f (well %.0f e-)",
                  s.ElectronsToDN(50000.0, rng), s.full_well_e);
    Check(s.ElectronsToDN(50000.0, rng) == 1000.0, "full well clamps before readout",
          detail);
  }

  // --- Dark current --------------------------------------------------------
  // On a black frame the signal is dark current alone, so the mean must scale
  // linearly with exposure time.
  {
    SensorModel s;
    s.noise.shot_noise = false;
    s.noise.read_noise = false;
    s.noise.dark_current = true;
    s.noise.dark_current_e_per_s = 500.0;
    s.gain_e_per_dn = 1.0;
    s.bit_depth = 16;

    auto mean_dark = [&](double t) {
      s.exposure_time_s = t;
      double sum = 0;
      for (int i = 0; i < 20000; i++) sum += s.ElectronsToDN(0.0, rng);
      return sum / 20000.0;
    };
    const double d1 = mean_dark(0.01);
    const double d4 = mean_dark(0.04);
    char detail[160];
    std::snprintf(detail, sizeof(detail), "t=0.01 -> %.2f DN, t=0.04 -> %.2f DN, ratio %.3f",
                  d1, d4, d4 / d1);
    Check(std::fabs(d4 / d1 - 4.0) < 0.05,
          "dark current scales linearly with exposure", detail);
  }

  std::printf("%s\n",
              failures == 0 ? "all sensor checks passed" : "SENSOR CHECKS FAILED");
  return failures == 0 ? 0 : 1;
}
