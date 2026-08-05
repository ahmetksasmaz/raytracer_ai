// Self-checks for the spectral core.
//
// These guard reference data and colour maths that nothing else in the suite
// can catch: a rendered image will look perfectly plausible with a mistyped
// digit in the CIE tables or an illuminant, and the error would show up only as
// a quiet bias in white-balance results. Every check here is against a
// published constant or an exact analytic identity.

#include <cmath>
#include <cstdio>
#include <string>

#include "../include/Spectrum.hpp"

namespace {

int failures = 0;

void Check(bool ok, const std::string& what, const std::string& detail) {
  std::printf("  %-46s %s   %s\n", what.c_str(), ok ? "PASS" : "FAIL",
              detail.c_str());
  if (!ok) failures++;
}

// CIE xy chromaticity of a spectrum.
void Chromaticity(const Spectrum& s, FP_PRECISION& x, FP_PRECISION& y) {
  FP_PRECISION X, Y, Z;
  SpectrumToXYZ(s, X, Y, Z);
  const FP_PRECISION sum = X + Y + Z;
  x = sum > 1e-12 ? X / sum : 0.0;
  y = sum > 1e-12 ? Y / sum : 0.0;
}

void CheckChromaticity(const char* name, const Spectrum& s, FP_PRECISION want_x,
                       FP_PRECISION want_y, FP_PRECISION tol) {
  FP_PRECISION x, y;
  Chromaticity(s, x, y);
  char detail[160];
  std::snprintf(detail, sizeof(detail),
                "got (%.4f, %.4f) want (%.4f, %.4f) tol %.4f", x, y, want_x,
                want_y, tol);
  Check(std::fabs(x - want_x) < tol && std::fabs(y - want_y) < tol,
        std::string("illuminant ") + name + " chromaticity", detail);
}

}  // namespace

int main() {
  std::printf("spectral core self-checks (%d bands, %.0f-%.0f nm)\n",
              kSpectralBands, kLambdaMin, kLambdaMax);

  // The CIE colour matching functions are defined to have equal integrals.
  // A transcription error in any of the three shows up here immediately.
  {
    FP_PRECISION sx = 0, sy = 0, sz = 0;
    for (int i = 0; i < kSpectralBands; i++) {
      sx += CIE_X()[i];
      sy += CIE_Y()[i];
      sz += CIE_Z()[i];
    }
    char detail[160];
    std::snprintf(detail, sizeof(detail), "X/Y=%.4f Z/Y=%.4f (both ~1)", sx / sy,
                  sz / sy);
    Check(std::fabs(sx / sy - 1.0) < 0.02 && std::fabs(sz / sy - 1.0) < 0.02,
          "CIE matching functions have equal integrals", detail);
  }

  // Published chromaticities. These are the checks that catch a bad digit in
  // the illuminant tables.
  CheckChromaticity("D65", IlluminantD65(), 0.3127, 0.3290, 0.006);
  CheckChromaticity("A", IlluminantA(), 0.4476, 0.4074, 0.006);
  CheckChromaticity("E", IlluminantE(), 1.0 / 3.0, 1.0 / 3.0, 0.004);

  // A flat unit spectrum must land on exactly (1,1,1) in linear sRGB. Every
  // neutral scene in the suite depends on this being exact, not approximate.
  {
    const Vec3f rgb = SpectrumToLinearSRGB(Spectrum::Constant(1.0));
    char detail[160];
    std::snprintf(detail, sizeof(detail), "(%.12f, %.12f, %.12f)", rgb.x, rgb.y,
                  rgb.z);
    Check(std::fabs(rgb.x - 1.0) < 1e-9 && std::fabs(rgb.y - 1.0) < 1e-9 &&
              std::fabs(rgb.z - 1.0) < 1e-9,
          "flat unit spectrum -> linear sRGB (1,1,1)", detail);
  }

  // Luminance of a flat unit spectrum is exactly 1 by the ybar normalisation.
  {
    const FP_PRECISION lum = SpectrumLuminance(Spectrum::Constant(1.0));
    char detail[80];
    std::snprintf(detail, sizeof(detail), "%.12f", lum);
    Check(std::fabs(lum - 1.0) < 1e-9, "luminance(flat 1.0) == 1", detail);
  }

  // Neutral RGB must survive the uplift exactly, since that is what keeps the
  // rendering tests bit-stable across the spectral conversion.
  {
    FP_PRECISION worst = 0.0;
    for (FP_PRECISION g : {0.0, 0.18, 0.5, 0.8, 1.0}) {
      const Vec3f out = SpectrumToLinearSRGB(UpliftRGB(Vec3f{g, g, g}));
      worst = std::max({worst, std::fabs(out.x - g), std::fabs(out.y - g),
                        std::fabs(out.z - g)});
    }
    char detail[80];
    std::snprintf(detail, sizeof(detail), "worst error %.3e", worst);
    Check(worst < 1e-9, "neutral RGB round-trips through uplift exactly", detail);
  }

  // Uplifted reflectances must be physically valid: no negative power.
  {
    FP_PRECISION most_negative = 0.0;
    for (FP_PRECISION r : {0.0, 0.5, 1.0})
      for (FP_PRECISION g : {0.0, 0.5, 1.0})
        for (FP_PRECISION b : {0.0, 0.5, 1.0}) {
          const Spectrum s = UpliftRGB(Vec3f{r, g, b});
          for (int i = 0; i < kSpectralBands; i++)
            most_negative = std::min(most_negative, s[i]);
        }
    char detail[80];
    std::snprintf(detail, sizeof(detail), "min band %.3e", most_negative);
    Check(most_negative > -1e-9, "uplifted spectra are non-negative", detail);
  }

  // Spectral products are not reproducible in RGB. Red light on a green surface
  // is exactly black in RGB; spectrally it is not, because the two spectra
  // overlap. If this ever reads zero the pipeline has silently collapsed to RGB.
  {
    const Spectrum product =
        hadamard(UpliftRGB(Vec3f{1, 0, 0}), UpliftRGB(Vec3f{0, 1, 0}));
    FP_PRECISION energy = 0.0;
    for (int i = 0; i < kSpectralBands; i++) energy += product[i];
    char detail[80];
    std::snprintf(detail, sizeof(detail), "band energy %.4f (RGB would give 0)",
                  energy);
    Check(energy > 1e-3, "spectral overlap survives where RGB gives black",
          detail);
  }

  // Resampling identity: a table already on the band grid must come back
  // unchanged.
  {
    std::vector<FP_PRECISION> wl, val;
    for (int i = 0; i < kSpectralBands; i++) {
      wl.push_back(BandWavelength(i));
      val.push_back(0.1 + 0.02 * i);
    }
    const Spectrum s = ResampleSpectrum(wl, val);
    FP_PRECISION worst = 0.0;
    for (int i = 0; i < kSpectralBands; i++)
      worst = std::max(worst, std::fabs(s[i] - val[i]));
    char detail[80];
    std::snprintf(detail, sizeof(detail), "worst error %.3e", worst);
    Check(worst < 1e-12, "resampling onto the same grid is identity", detail);
  }

  std::printf("%s\n", failures == 0 ? "all spectral checks passed"
                                    : "SPECTRAL CHECKS FAILED");
  return failures == 0 ? 0 : 1;
}
