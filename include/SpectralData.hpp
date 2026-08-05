#pragma once

// Tabulated spectral reference data, at its own master resolution.
//
// Everything here is stored as an explicit (wavelength, value) table rather
// than pre-sampled onto the renderer's band grid. That decoupling is the point:
// Spectrum.hpp resamples these at start-up, so kSpectralBands can be changed
// without touching any of this data.
//
// This header deliberately has NO dependency on Spectrum, which is what keeps
// that direction of the dependency acyclic.

#include <vector>

#include "../extern/parser.h"

namespace spectral_data {

// ---------------------------------------------------------------------------
// CIE 1931 2-degree standard observer, 380-780 nm at 10 nm.
// ---------------------------------------------------------------------------

inline constexpr int kCIECount = 41;

inline constexpr FP_PRECISION kCIELambda[kCIECount] = {
    380, 390, 400, 410, 420, 430, 440, 450, 460, 470, 480, 490, 500, 510,
    520, 530, 540, 550, 560, 570, 580, 590, 600, 610, 620, 630, 640, 650,
    660, 670, 680, 690, 700, 710, 720, 730, 740, 750, 760, 770, 780};

inline constexpr FP_PRECISION kCIEX[kCIECount] = {
    0.001368, 0.004243, 0.014310, 0.043510, 0.134380, 0.283900, 0.348280,
    0.336200, 0.290800, 0.195360, 0.095640, 0.032010, 0.004900, 0.009300,
    0.063270, 0.165500, 0.290400, 0.433450, 0.594500, 0.762100, 0.916300,
    1.026300, 1.062200, 1.002600, 0.854450, 0.642400, 0.447900, 0.283500,
    0.164900, 0.087400, 0.046770, 0.022700, 0.011359, 0.005790, 0.002899,
    0.001440, 0.000690, 0.000332, 0.000166, 0.000083, 0.000042};

inline constexpr FP_PRECISION kCIEY[kCIECount] = {
    0.000039, 0.000120, 0.000396, 0.001210, 0.004000, 0.011600, 0.023000,
    0.038000, 0.060000, 0.090980, 0.139020, 0.208020, 0.323000, 0.503000,
    0.710000, 0.862000, 0.954000, 0.994950, 0.995000, 0.952000, 0.870000,
    0.757000, 0.631000, 0.503000, 0.381000, 0.265000, 0.175000, 0.107000,
    0.061000, 0.032000, 0.017000, 0.008210, 0.004102, 0.002091, 0.001047,
    0.000520, 0.000249, 0.000120, 0.000060, 0.000030, 0.000015};

inline constexpr FP_PRECISION kCIEZ[kCIECount] = {
    0.006450, 0.020050, 0.067850, 0.207400, 0.645600, 1.385600, 1.747060,
    1.772110, 1.669200, 1.287640, 0.812950, 0.465180, 0.272000, 0.158200,
    0.078250, 0.042160, 0.020300, 0.008750, 0.003900, 0.002100, 0.001650,
    0.001100, 0.000800, 0.000340, 0.000190, 0.000050, 0.000020, 0.000000,
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000,
    0.000000, 0.000000, 0.000000, 0.000000, 0.000000, 0.000000};

// ---------------------------------------------------------------------------
// CIE standard illuminants, relative spectral power, same 380-780 nm grid.
//
// Each is validated at start-up against its published chromaticity (see
// tests/): D65 -> (0.3127, 0.3290), A -> (0.4476, 0.4074), E -> (1/3, 1/3).
// That check exists specifically to catch a transcription error in these
// tables, which would otherwise silently bias every white-balance result.
// ---------------------------------------------------------------------------

inline constexpr FP_PRECISION kD65[kCIECount] = {
    49.98, 54.65, 82.75, 91.49, 93.43, 86.68, 104.87, 117.01, 117.81, 114.86,
    115.92, 108.81, 109.35, 107.80, 104.79, 107.69, 104.41, 104.05, 100.00,
    96.33, 95.79, 88.69, 90.01, 89.60, 87.70, 83.29, 83.70, 80.03, 80.21,
    82.28, 78.28, 69.72, 71.61, 74.35, 61.60, 69.89, 75.09, 63.59, 46.42,
    66.81, 63.38};

inline constexpr FP_PRECISION kIlluminantA[kCIECount] = {
    9.80, 12.09, 14.71, 17.68, 21.00, 24.67, 28.70, 33.09, 37.81, 42.87,
    48.24, 53.91, 59.86, 66.06, 72.50, 79.13, 85.95, 92.91, 100.00, 107.18,
    114.44, 121.73, 129.04, 136.35, 143.62, 150.84, 157.98, 165.03, 171.96,
    178.77, 185.43, 191.93, 198.26, 204.41, 210.36, 216.12, 221.67, 227.00,
    232.12, 237.01, 241.68};

// ---------------------------------------------------------------------------
// Reflectance data.
//
// IMPORTANT: the ColorChecker patches below are NOT measured spectra. They are
// the published sRGB values, which the renderer uplifts to smooth spectra like
// any other RGB input. That is adequate for wiring up and sanity-checking a
// pipeline, but it is NOT adequate for research conclusions: an uplifted
// spectrum is one of infinitely many metamers of the same colour, and telling
// metamers apart is precisely what a spectral white-balance study is for.
//
// For real work, supply measured reflectances through the scene file's explicit
// spectrum syntax. Storing genuinely measured data here would mean transcribing
// 24 x 36 values, where a single wrong digit would quietly corrupt results, so
// it is deliberately left to the user to provide from an authoritative source.
// ---------------------------------------------------------------------------

struct NamedRGB {
  const char* name;
  FP_PRECISION r, g, b;
};

inline constexpr int kColorCheckerCount = 24;

inline constexpr NamedRGB kColorCheckerSRGB[kColorCheckerCount] = {
    {"dark_skin", 0.4500, 0.3200, 0.2667},
    {"light_skin", 0.7608, 0.5882, 0.5098},
    {"blue_sky", 0.3647, 0.4784, 0.6118},
    {"foliage", 0.3451, 0.4235, 0.2667},
    {"blue_flower", 0.5098, 0.5020, 0.6902},
    {"bluish_green", 0.3882, 0.7412, 0.6706},
    {"orange", 0.8510, 0.4863, 0.1765},
    {"purplish_blue", 0.2745, 0.3569, 0.6392},
    {"moderate_red", 0.7569, 0.3333, 0.3725},
    {"purple", 0.3647, 0.2353, 0.4157},
    {"yellow_green", 0.6235, 0.7412, 0.2471},
    {"orange_yellow", 0.8902, 0.6353, 0.1765},
    {"blue", 0.2000, 0.2431, 0.5804},
    {"green", 0.2745, 0.5882, 0.2863},
    {"red", 0.6902, 0.1922, 0.2235},
    {"yellow", 0.9333, 0.7804, 0.1255},
    {"magenta", 0.7333, 0.3294, 0.5843},
    {"cyan", 0.0000, 0.5216, 0.6510},
    {"white_9_5", 0.9490, 0.9490, 0.9412},
    {"neutral_8", 0.7843, 0.7843, 0.7843},
    {"neutral_6_5", 0.6275, 0.6275, 0.6275},
    {"neutral_5", 0.4745, 0.4745, 0.4745},
    {"neutral_3_5", 0.3294, 0.3294, 0.3294},
    {"black_2", 0.2000, 0.2000, 0.2000}};

// Helpers to hand a table to ResampleSpectrum without copying the constants
// around at every call site.
inline std::vector<FP_PRECISION> Lambda() {
  return std::vector<FP_PRECISION>(kCIELambda, kCIELambda + kCIECount);
}

inline std::vector<FP_PRECISION> Values(const FP_PRECISION (&table)[kCIECount]) {
  return std::vector<FP_PRECISION>(table, table + kCIECount);
}

}  // namespace spectral_data
