#pragma once

// Spectral radiance / reflectance carried as N discrete bands.
//
// The renderer used to carry radiance as an RGB Vec3f. That cannot represent a
// colour-filter-array transmittance or a sensor quantum-efficiency curve: any
// such filter applied to a colour already collapsed to three numbers is just a
// 3x3 matrix, so results would measure the matrix rather than the physics.
// Everything radiometric is therefore carried as a Spectrum instead.
//
// Spectrum is a DISTINCT type, deliberately not a typedef of Vec3f, so the
// compiler locates every radiometric site during the conversion rather than
// silently mixing spectral and RGB quantities.
//
// Geometry (positions, normals, directions) stays Vec3f.

#include <algorithm>
#include <cmath>
#include <string>

#include "../extern/parser.h"
#include "SpectralData.hpp"

using namespace parser;

// Band layout. 31 samples at 10 nm spacing covering 400-700 nm inclusive, each
// treated as the centre of a 10 nm-wide band. Compile-time so Spectrum stays a
// flat value type.
//
// kSpectralBands IS a free knob: all reference data lives in SpectralData.hpp
// at its own master resolution and is resampled onto whatever grid is
// configured here, so changing this constant needs no other edits.
inline constexpr int kSpectralBands = 31;
inline constexpr FP_PRECISION kLambdaMin = 400.0;
inline constexpr FP_PRECISION kLambdaMax = 700.0;
inline constexpr FP_PRECISION kBandWidth =
    (kLambdaMax - kLambdaMin) / (kSpectralBands - 1);

// Centre wavelength of band i, in nanometres.
inline constexpr FP_PRECISION BandWavelength(int i) {
  return kLambdaMin + i * kBandWidth;
}

struct Spectrum {
  FP_PRECISION v[kSpectralBands];

  Spectrum() {
    for (int i = 0; i < kSpectralBands; i++) v[i] = 0.0;
  }

  explicit Spectrum(FP_PRECISION constant_value) {
    for (int i = 0; i < kSpectralBands; i++) v[i] = constant_value;
  }

  static Spectrum Zero() { return Spectrum(); }
  static Spectrum Constant(FP_PRECISION x) { return Spectrum(x); }

  FP_PRECISION& operator[](int i) { return v[i]; }
  const FP_PRECISION& operator[](int i) const { return v[i]; }

  bool IsBlack() const {
    for (int i = 0; i < kSpectralBands; i++) {
      if (v[i] != 0.0) return false;
    }
    return true;
  }

  FP_PRECISION MaxComponent() const {
    FP_PRECISION m = v[0];
    for (int i = 1; i < kSpectralBands; i++) m = std::max(m, v[i]);
    return m;
  }

  bool IsFinite() const {
    for (int i = 0; i < kSpectralBands; i++) {
      if (!std::isfinite(v[i])) return false;
    }
    return true;
  }

  // Replace any non-finite band with zero. The path tracer guards its results
  // this way; doing it per band keeps one bad wavelength from poisoning the
  // whole sample.
  Spectrum& SanitizeInPlace() {
    for (int i = 0; i < kSpectralBands; i++) {
      if (!std::isfinite(v[i])) v[i] = 0.0;
    }
    return *this;
  }

  // Firefly clamp, applied per band rather than per RGB channel. Clamping bands
  // is the spectrally meaningful operation; the RGB version could only limit an
  // already-collapsed colour.
  Spectrum& ClampMaxInPlace(FP_PRECISION limit) {
    if (limit <= 0.0) return *this;
    for (int i = 0; i < kSpectralBands; i++) v[i] = std::min(v[i], limit);
    return *this;
  }
};

// Arithmetic mirrors the Vec3f operators in Helper.hpp so call sites read the
// same after the conversion.

inline Spectrum operator+(const Spectrum& a, const Spectrum& b) {
  Spectrum r;
  for (int i = 0; i < kSpectralBands; i++) r.v[i] = a.v[i] + b.v[i];
  return r;
}

inline Spectrum operator-(const Spectrum& a, const Spectrum& b) {
  Spectrum r;
  for (int i = 0; i < kSpectralBands; i++) r.v[i] = a.v[i] - b.v[i];
  return r;
}

inline Spectrum operator*(const Spectrum& a, FP_PRECISION s) {
  Spectrum r;
  for (int i = 0; i < kSpectralBands; i++) r.v[i] = a.v[i] * s;
  return r;
}

inline Spectrum operator*(FP_PRECISION s, const Spectrum& a) { return a * s; }

inline Spectrum operator/(const Spectrum& a, FP_PRECISION s) {
  Spectrum r;
  for (int i = 0; i < kSpectralBands; i++) r.v[i] = a.v[i] / s;
  return r;
}

// Per-band product. Named to match the Vec3f helper it replaces; this is the
// operation used whenever a reflectance modulates a radiance.
inline Spectrum hadamard(const Spectrum& a, const Spectrum& b) {
  Spectrum r;
  for (int i = 0; i < kSpectralBands; i++) r.v[i] = a.v[i] * b.v[i];
  return r;
}

inline Spectrum& operator+=(Spectrum& a, const Spectrum& b) {
  for (int i = 0; i < kSpectralBands; i++) a.v[i] += b.v[i];
  return a;
}

inline Spectrum& operator*=(Spectrum& a, FP_PRECISION s) {
  for (int i = 0; i < kSpectralBands; i++) a.v[i] *= s;
  return a;
}

// Resample an arbitrary (wavelength, value) table onto the band grid by linear
// interpolation, clamping outside the table's range. Used for scene-supplied
// spectra and for the tabulated illuminant / sensor curves.
inline Spectrum ResampleSpectrum(const std::vector<FP_PRECISION>& wavelengths,
                                 const std::vector<FP_PRECISION>& values) {
  Spectrum result;
  if (wavelengths.empty() || wavelengths.size() != values.size()) return result;

  for (int i = 0; i < kSpectralBands; i++) {
    const FP_PRECISION lambda = BandWavelength(i);
    if (lambda <= wavelengths.front()) {
      result.v[i] = values.front();
      continue;
    }
    if (lambda >= wavelengths.back()) {
      result.v[i] = values.back();
      continue;
    }
    // wavelengths are assumed ascending
    size_t hi = 1;
    while (hi < wavelengths.size() && wavelengths[hi] < lambda) hi++;
    const FP_PRECISION w0 = wavelengths[hi - 1];
    const FP_PRECISION w1 = wavelengths[hi];
    const FP_PRECISION t = (w1 > w0) ? (lambda - w0) / (w1 - w0) : 0.0;
    result.v[i] = values[hi - 1] * (1.0 - t) + values[hi] * t;
  }
  return result;
}

// ---------------------------------------------------------------------------
// CIE 1931 2-degree standard observer, resampled onto the band grid.
//
// Built once from the master tables in SpectralData.hpp rather than tabulated
// at the working resolution, so the band layout stays configurable.
// ---------------------------------------------------------------------------

inline const Spectrum& CIE_X() {
  static const Spectrum s = ResampleSpectrum(spectral_data::Lambda(),
                                             spectral_data::Values(spectral_data::kCIEX));
  return s;
}
inline const Spectrum& CIE_Y() {
  static const Spectrum s = ResampleSpectrum(spectral_data::Lambda(),
                                             spectral_data::Values(spectral_data::kCIEY));
  return s;
}
inline const Spectrum& CIE_Z() {
  static const Spectrum s = ResampleSpectrum(spectral_data::Lambda(),
                                             spectral_data::Values(spectral_data::kCIEZ));
  return s;
}

// Standard illuminants, likewise resampled. Relative power, normalised to 1.0
// at 560 nm so they can be scaled by a scene-supplied intensity.
inline Spectrum NormalizeAt560(Spectrum s) {
  const int idx = static_cast<int>((560.0 - kLambdaMin) / kBandWidth + 0.5);
  if (idx >= 0 && idx < kSpectralBands && s[idx] > 1e-12) s = s / s[idx];
  return s;
}

inline const Spectrum& IlluminantD65() {
  static const Spectrum s = NormalizeAt560(ResampleSpectrum(
      spectral_data::Lambda(), spectral_data::Values(spectral_data::kD65)));
  return s;
}
inline const Spectrum& IlluminantA() {
  static const Spectrum s = NormalizeAt560(ResampleSpectrum(
      spectral_data::Lambda(), spectral_data::Values(spectral_data::kIlluminantA)));
  return s;
}
inline const Spectrum& IlluminantE() {
  static const Spectrum s = Spectrum::Constant(1.0);
  return s;
}

// Look an illuminant up by the name a scene file uses. Returns false when the
// name is unknown so the caller can report it rather than silently rendering
// under the wrong light.
inline bool IlluminantByName(const std::string& name, Spectrum& out) {
  if (name == "D65") { out = IlluminantD65(); return true; }
  if (name == "A")   { out = IlluminantA();   return true; }
  if (name == "E")   { out = IlluminantE();   return true; }
  return false;
}

// Integral of ybar over the band grid, used to normalise Y so that a flat unit
// spectrum has luminance exactly 1.
inline FP_PRECISION CIE_Y_Integral() {
  FP_PRECISION sum = 0.0;
  for (int i = 0; i < kSpectralBands; i++) sum += CIE_Y()[i];
  return sum;
}

// Spectrum -> CIE XYZ, normalised by the ybar integral. A flat unit spectrum
// therefore yields Y = 1 (and X, Z close to 1, i.e. equal-energy white E).
inline void SpectrumToXYZ(const Spectrum& s, FP_PRECISION& X, FP_PRECISION& Y,
                          FP_PRECISION& Z) {
  FP_PRECISION x_sum = 0.0, y_sum = 0.0, z_sum = 0.0;
  for (int i = 0; i < kSpectralBands; i++) {
    x_sum += s.v[i] * CIE_X()[i];
    y_sum += s.v[i] * CIE_Y()[i];
    z_sum += s.v[i] * CIE_Z()[i];
  }
  const FP_PRECISION norm = CIE_Y_Integral();
  X = x_sum / norm;
  Y = y_sum / norm;
  Z = z_sum / norm;
}

// CIE XYZ -> linear sRGB primaries (standard sRGB / Rec.709 matrix, D65).
inline Vec3f XYZToLinearSRGB(FP_PRECISION X, FP_PRECISION Y, FP_PRECISION Z) {
  return Vec3f{ 3.2404542 * X - 1.5371385 * Y - 0.4985314 * Z,
               -0.9692660 * X + 1.8760108 * Y + 0.0415560 * Z,
                0.0556434 * X - 0.2040259 * Y + 1.0572252 * Z};
}

// Spectrum -> linear sRGB for the conventional (non-sensor) image output.
//
// The sRGB matrix is defined against a D65 white, but a flat unit spectrum is
// equal-energy white E, so feeding it through the raw matrix would tint it.
// Dividing by the RGB of a flat spectrum applies a von Kries adaptation from E
// to the working white, which both removes that tint and guarantees the
// property the existing neutral-scene tests depend on: a flat unit spectrum
// maps to exactly (1, 1, 1).
//
// This affects only the legacy RGB output path. The sensor pipeline consumes
// the spectrum directly and never passes through here.
inline Vec3f SpectrumToLinearSRGB(const Spectrum& s) {
  static const Vec3f white_normalizer = [] {
    FP_PRECISION X, Y, Z;
    SpectrumToXYZ(Spectrum::Constant(1.0), X, Y, Z);
    return XYZToLinearSRGB(X, Y, Z);
  }();

  FP_PRECISION X, Y, Z;
  SpectrumToXYZ(s, X, Y, Z);
  const Vec3f rgb = XYZToLinearSRGB(X, Y, Z);
  return Vec3f{rgb.x / white_normalizer.x, rgb.y / white_normalizer.y,
               rgb.z / white_normalizer.z};
}

// Luminance (CIE Y) of a spectrum, on the same normalisation as above.
inline FP_PRECISION SpectrumLuminance(const Spectrum& s) {
  FP_PRECISION X, Y, Z;
  SpectrumToXYZ(s, X, Y, Z);
  return Y;
}

// ---------------------------------------------------------------------------
// RGB -> spectrum uplifting (Smits 1999).
//
// Every existing scene authors reflectance and emission as RGB. Uplifting turns
// those into smooth, plausible, non-negative spectra so old scenes keep working
// unchanged once the renderer is spectral. Scene files can supply measured
// spectra instead, which take precedence -- uplifting is the fallback, not the
// intended path for research work.
//
// The construction matters for one specific reason: for a NEUTRAL colour
// (r == g == b) Smits returns white * r, i.e. a flat spectrum. Flat spectra
// convert back to exactly the same RGB, so every neutral scene -- which is all
// of the existing correctness tests -- is unchanged by the spectral conversion.
// ---------------------------------------------------------------------------

namespace smits {

// Smits' basis, 10 equal bins spanning 380-720 nm.
inline constexpr int kBins = 10;
inline constexpr FP_PRECISION kBinLambdaMin = 380.0;
inline constexpr FP_PRECISION kBinLambdaMax = 720.0;

// Smits' published white basis dips very slightly (0.9992-1.0) as a by-product
// of his least-squares fit. It is replaced here with an exactly flat curve: a
// neutral grey physically IS a wavelength-flat reflectance, so this is the more
// correct choice, and it makes neutral colours round-trip through the uplift
// exactly rather than to within 5e-4. Every existing correctness test is
// neutral, so that exactness is what keeps them bit-stable.
inline constexpr FP_PRECISION kWhite[kBins] = {
    1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0, 1.0};
inline constexpr FP_PRECISION kCyan[kBins] = {
    0.9710, 0.9426, 1.0007, 1.0007, 1.0007, 1.0007, 0.1564, 0.0000, 0.0000, 0.0000};
inline constexpr FP_PRECISION kMagenta[kBins] = {
    1.0000, 1.0000, 0.9685, 0.2229, 0.0000, 0.0458, 0.8369, 1.0000, 1.0000, 0.9959};
inline constexpr FP_PRECISION kYellow[kBins] = {
    0.0001, 0.0000, 0.1088, 0.6651, 1.0000, 1.0000, 0.9996, 0.9586, 0.9685, 0.9840};
inline constexpr FP_PRECISION kRed[kBins] = {
    0.1012, 0.0515, 0.0000, 0.0000, 0.0000, 0.0000, 0.8325, 1.0149, 1.0149, 1.0149};
inline constexpr FP_PRECISION kGreen[kBins] = {
    0.0000, 0.0000, 0.0273, 0.7937, 1.0000, 0.9418, 0.1719, 0.0000, 0.0000, 0.0025};
inline constexpr FP_PRECISION kBlue[kBins] = {
    1.0000, 1.0000, 0.8916, 0.3323, 0.0000, 0.0000, 0.0003, 0.0369, 0.0483, 0.0496};

// Resample one basis curve onto the renderer's band grid.
inline Spectrum ToBands(const FP_PRECISION (&basis)[kBins]) {
  std::vector<FP_PRECISION> wavelengths(kBins), values(kBins);
  const FP_PRECISION bin_width = (kBinLambdaMax - kBinLambdaMin) / kBins;
  for (int i = 0; i < kBins; i++) {
    wavelengths[i] = kBinLambdaMin + (i + 0.5) * bin_width;  // bin centre
    values[i] = basis[i];
  }
  return ResampleSpectrum(wavelengths, values);
}

}  // namespace smits

// Uplift a linear RGB reflectance or emission to a smooth spectrum.
inline Spectrum UpliftRGB(const Vec3f& rgb) {
  static const Spectrum white = smits::ToBands(smits::kWhite);
  static const Spectrum cyan = smits::ToBands(smits::kCyan);
  static const Spectrum magenta = smits::ToBands(smits::kMagenta);
  static const Spectrum yellow = smits::ToBands(smits::kYellow);
  static const Spectrum red = smits::ToBands(smits::kRed);
  static const Spectrum green = smits::ToBands(smits::kGreen);
  static const Spectrum blue = smits::ToBands(smits::kBlue);

  const FP_PRECISION r = rgb.x, g = rgb.y, b = rgb.z;
  Spectrum result;

  // Take out the largest achromatic part first, then the remaining secondary
  // and primary, so the result stays smooth and non-negative.
  if (r <= g && r <= b) {
    result += white * r;
    if (g <= b) {
      result += cyan * (g - r);
      result += blue * (b - g);
    } else {
      result += cyan * (b - r);
      result += green * (g - b);
    }
  } else if (g <= r && g <= b) {
    result += white * g;
    if (r <= b) {
      result += magenta * (r - g);
      result += blue * (b - r);
    } else {
      result += magenta * (b - g);
      result += red * (r - b);
    }
  } else {
    result += white * b;
    if (r <= g) {
      result += yellow * (r - b);
      result += green * (g - r);
    } else {
      result += yellow * (g - b);
      result += red * (r - g);
    }
  }

  return result;
}
