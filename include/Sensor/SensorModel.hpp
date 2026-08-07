#pragma once

// Camera sensor simulation: spectral radiance in, digital numbers out.
//
// The chain deliberately mirrors a real sensor, one physical stage at a time,
// because each stage is something a calibration or white-balance method has to
// cope with:
//
//   spectral radiance  ->  photons        (radiometry: aperture, pixel area,
//                                          exposure, and photon energy hc/lambda)
//   photons            ->  electrons      (quantum efficiency x CFA transmittance
//                                          for whichever Bayer channel this pixel
//                                          carries, integrated over wavelength)
//   electrons          ->  noisy electrons (Poisson shot, Poisson dark current,
//                                          Gaussian read)
//   noisy electrons    ->  digital number (full-well clamp, gain, quantisation)
//
// Keeping the spectral radiance image separate from this model is the point:
// one render can be replayed through any number of different sensors.

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

#include "Helper.hpp"
#include "Spectrum.hpp"

// Colour filter array layout, named by the 2x2 tile read in row-major order
// starting at pixel (0,0).
enum class BayerPattern { kRGGB, kBGGR, kGRBG, kGBRG };

// A Gaussian band-pass, used only to synthesise placeholder filters.
inline Spectrum GaussianFilter(FP_PRECISION peak_nm, FP_PRECISION sigma_nm,
                               FP_PRECISION peak_transmittance) {
  Spectrum s;
  for (int band = 0; band < kSpectralBands; band++) {
    const FP_PRECISION d = BandWavelength(band) - peak_nm;
    s[band] = peak_transmittance * std::exp(-(d * d) / (2.0 * sigma_nm * sigma_nm));
  }
  return s;
}

// Stand-in Bayer filter transmittances.
//
// These are smooth Gaussians at plausible centre wavelengths, NOT a measured
// CFA. They exist so a scene without explicit filter data still renders
// something sensible. Any result that depends on the shape of the colour
// filters -- which a white-balance study does -- needs measured curves supplied
// through the scene file instead.
inline void DefaultBayerCFA(Spectrum out[3]) {
  out[0] = GaussianFilter(600.0, 40.0, 0.95);  // red
  out[1] = GaussianFilter(540.0, 40.0, 0.95);  // green
  out[2] = GaussianFilter(460.0, 40.0, 0.95);  // blue
}

enum class SensorChannel { kRed = 0, kGreen = 1, kBlue = 2 };

struct SensorNoiseSettings {
  bool shot_noise = true;         // Poisson on signal electrons
  bool read_noise = true;         // Gaussian, in electrons
  bool dark_current = true;       // Poisson, scaled by exposure time
  FP_PRECISION read_noise_sigma_e = 2.0;      // electrons RMS
  FP_PRECISION dark_current_e_per_s = 5.0;    // electrons per second per pixel
};

struct SensorModel {
  // Spectral response. quantum_efficiency is the sensor's QE(lambda); cfa[c] is
  // the transmittance of the colour filter over channel c. They are kept apart
  // rather than pre-multiplied so a study can vary one without the other.
  Spectrum quantum_efficiency = Spectrum::Constant(0.5);
  Spectrum cfa[3];

  BayerPattern pattern = BayerPattern::kRGGB;

  // Radiometry.
  FP_PRECISION exposure_time_s = 0.01;
  FP_PRECISION pixel_pitch_m = 3.45e-6;   // square pixel edge
  FP_PRECISION f_number = 2.8;            // sets the solid angle the pixel sees

  // Conversion and quantisation.
  FP_PRECISION full_well_e = 10000.0;
  FP_PRECISION gain_e_per_dn = 1.0;
  int bit_depth = 12;

  // Output dynamic range, in stops below saturation.
  //
  // The display mapping is FIXED, not fitted to the frame: saturation is always
  // the top of the ADC range and black is always that many stops below it. A
  // camera does not re-expose itself to suit the scene, so neither does this --
  // point it at something too bright and highlights blow out, point it at
  // something too dark and shadows crush. Both are correct results, not faults
  // in the render.
  //
  // <= 0 means derive it from the sensor itself; see DefaultDynamicRangeStops.
  FP_PRECISION dynamic_range_stops = 0.0;

  SensorNoiseSettings noise;

  // Which colour the CFA puts over this pixel.
  SensorChannel ChannelAt(int x, int y) const {
    const int col = x & 1;
    const int row = y & 1;
    switch (pattern) {
      case BayerPattern::kRGGB:
        if (row == 0) return col == 0 ? SensorChannel::kRed : SensorChannel::kGreen;
        return col == 0 ? SensorChannel::kGreen : SensorChannel::kBlue;
      case BayerPattern::kBGGR:
        if (row == 0) return col == 0 ? SensorChannel::kBlue : SensorChannel::kGreen;
        return col == 0 ? SensorChannel::kGreen : SensorChannel::kRed;
      case BayerPattern::kGRBG:
        if (row == 0) return col == 0 ? SensorChannel::kGreen : SensorChannel::kRed;
        return col == 0 ? SensorChannel::kBlue : SensorChannel::kGreen;
      case BayerPattern::kGBRG:
      default:
        if (row == 0) return col == 0 ? SensorChannel::kGreen : SensorChannel::kBlue;
        return col == 0 ? SensorChannel::kRed : SensorChannel::kGreen;
    }
  }

  FP_PRECISION MaxDN() const {
    return static_cast<FP_PRECISION>((1 << bit_depth) - 1);
  }

  // The sensor's own engineering dynamic range: the ratio between the largest
  // signal the well can hold and the smallest one read noise still leaves
  // detectable. This is the figure a datasheet quotes, so it is the honest
  // default -- a scene declaring nothing gets the range the modelled sensor
  // actually has.
  //
  // With no read noise the ratio is unbounded, so the ADC's own resolution
  // stands in: a 12-bit converter cannot express more than 12 stops regardless.
  FP_PRECISION DefaultDynamicRangeStops() const {
    if (noise.read_noise_sigma_e > 0.0 && full_well_e > 0.0) {
      return std::log2(full_well_e / noise.read_noise_sigma_e);
    }
    return static_cast<FP_PRECISION>(bit_depth);
  }

  FP_PRECISION DynamicRangeStops() const {
    return dynamic_range_stops > 0.0 ? dynamic_range_stops
                                     : DefaultDynamicRangeStops();
  }

  // Digital number at the bottom of the rendered range. Anything at or below it
  // reads as black.
  FP_PRECISION BlackDN() const {
    return MaxDN() * std::pow(static_cast<FP_PRECISION>(2.0),
                              -DynamicRangeStops());
  }

  // Fixed mapping from a digital number to [0,1]. Scene-independent by
  // construction: the only inputs are the sensor's own constants.
  //
  // Clipping here is per channel and happens BEFORE the colour matrix, which is
  // where it happens in a real camera too -- the well saturates one colour at a
  // time. That is what turns a blown red into the familiar clipped-highlight
  // hue shift rather than into neutral white.
  FP_PRECISION NormalizedFromDN(FP_PRECISION dn) const {
    const FP_PRECISION white = MaxDN();
    const FP_PRECISION black = BlackDN();
    if (!(white > black)) return dn >= white ? 1.0 : 0.0;
    const FP_PRECISION v = (dn - black) / (white - black);
    return std::min(std::max(v, static_cast<FP_PRECISION>(0.0)),
                    static_cast<FP_PRECISION>(1.0));
  }

  // Total spectral sensitivity of one channel: QE times that channel's filter.
  // This is what the sensorRGB -> XYZ fit is computed from.
  Spectrum ChannelSensitivity(SensorChannel channel) const {
    return hadamard(quantum_efficiency, cfa[static_cast<int>(channel)]);
  }

  // Photons arriving at one pixel per unit spectral radiance, per band.
  //
  //   photons = L * A * Omega * t * (lambda / hc) * bandwidth
  //
  // A is the pixel area and Omega the solid angle the aperture subtends, which
  // for an f-number N is approximately pi / (4 N^2). The lambda / hc factor is
  // the conversion from energy to photon count and is why this cannot be done
  // on an RGB triple: it is wavelength dependent.
  // The purely wavelength-dependent part: photons per joule, per band.
  //
  // Deliberately split from the geometry so it can be cached safely. Caching
  // the whole product in a function-local static is a trap -- it would be
  // computed once for the first sensor and silently reused for every other one,
  // making exposure time, pixel pitch and f-number have no effect from the
  // second sensor onwards.
  static const Spectrum& PhotonsPerJoulePerBand() {
    static const Spectrum table = [] {
      constexpr FP_PRECISION kPlanck = 6.62607015e-34;    // J s
      constexpr FP_PRECISION kLightSpeed = 2.99792458e8;  // m/s
      Spectrum s;
      for (int band = 0; band < kSpectralBands; band++) {
        const FP_PRECISION lambda_m = BandWavelength(band) * 1e-9;
        const FP_PRECISION photon_energy = kPlanck * kLightSpeed / lambda_m;
        // kBandWidth turns the per-nanometre density into a per-band total.
        s[band] = kBandWidth / photon_energy;
      }
      return s;
    }();
    return table;
  }

  // Everything configuration-dependent collapses to one scalar: pixel area
  // times the solid angle the aperture subtends (pi / 4N^2) times exposure.
  FP_PRECISION GeometryFactor() const {
    const FP_PRECISION area = pixel_pitch_m * pixel_pitch_m;
    const FP_PRECISION solid_angle = M_PI / (4.0 * f_number * f_number);
    return area * solid_angle * exposure_time_s;
  }

  Spectrum PhotonsPerUnitRadiance() const {
    return PhotonsPerJoulePerBand() * GeometryFactor();
  }

  // Noise-free electron count for one pixel.
  FP_PRECISION ElectronsAt(const Spectrum& radiance, int x, int y) const {
    const Spectrum& photons_per_joule = PhotonsPerJoulePerBand();
    const FP_PRECISION geometry = GeometryFactor();
    const Spectrum sensitivity = ChannelSensitivity(ChannelAt(x, y));

    FP_PRECISION electrons = 0.0;
    for (int band = 0; band < kSpectralBands; band++) {
      electrons += radiance[band] * photons_per_joule[band] * sensitivity[band];
    }
    return std::max(static_cast<FP_PRECISION>(0.0), electrons * geometry);
  }

  // Applies the noise chain and quantises. Kept separate from ElectronsAt so a
  // noise-free reference can be produced from the same signal.
  FP_PRECISION ElectronsToDN(FP_PRECISION signal_electrons) const {
    FP_PRECISION electrons = signal_electrons;

    if (noise.shot_noise) {
      electrons = SamplePoisson(electrons);
    }
    if (noise.dark_current) {
      electrons += SamplePoisson(noise.dark_current_e_per_s * exposure_time_s);
    }
    if (noise.read_noise) {
      electrons += SampleGaussian(0.0, noise.read_noise_sigma_e);
    }

    // Saturation happens in the well, before readout.
    electrons = std::min(electrons, full_well_e);

    const FP_PRECISION dn = electrons / gain_e_per_dn;
    // A real ADC cannot return a negative code, even though read noise can
    // drive a dark pixel below zero electrons.
    return std::min(std::max(std::floor(dn), static_cast<FP_PRECISION>(0.0)),
                    MaxDN());
  }
};
