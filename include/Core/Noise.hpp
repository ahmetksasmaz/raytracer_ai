#pragma once

// Random sampling primitives, split out of Helper.hpp.
//
// Helper.hpp seeds a thread_local generator from the thread id, which is right
// for a renderer -- every tile thread gets a different stream without any
// coordination -- but wrong for a sensor stage that is now its own executable:
// the same input file and the same config would produce a different RAW on
// every run, and there would be no way to reproduce a frame you had already
// looked at. So the generator is explicit here and the seed is a parameter.
//
// The generator itself is the same xorshift as Helper.hpp's, kept identical so
// a stage run through either path draws from the same sequence for a given
// state.

#include <cmath>
#include <cstdint>

#include "Types.hpp"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace core {

struct RandomGenerator {
  uint64_t state;

  explicit RandomGenerator(uint64_t seed = 0) {
    state = seed != 0 ? seed : 0x853c49e6748fea9bULL;
  }

  inline uint64_t Next() {
    uint64_t x = state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x;
    return x * 0x2545F4914F6CDD1DULL;
  }

  inline FP_PRECISION NextDouble() {
    return static_cast<FP_PRECISION>(Next() >> 11) * (1.0 / 9007199254740992.0);
  }

  // Standard normal via Box-Muller.
  inline FP_PRECISION Gaussian(FP_PRECISION mean, FP_PRECISION stddev) {
    if (stddev <= 0.0) return mean;
    // Guard the log against exactly zero.
    const FP_PRECISION u1 =
        std::max(NextDouble(), static_cast<FP_PRECISION>(1e-12));
    const FP_PRECISION u2 = NextDouble();
    return mean + stddev * std::sqrt(-2.0 * std::log(u1)) *
                      std::cos(2.0 * M_PI * u2);
  }

  // Poisson draw, used for photon shot noise and dark current.
  //
  // Two regimes, which is what makes this usable across the whole exposure
  // range: Knuth's product method is exact but costs O(lambda) draws, so it is
  // used only for small counts. Above the threshold the distribution is well
  // approximated by a Gaussian of matching mean and variance, which is the
  // standard treatment and avoids an unbounded loop on a bright pixel where
  // lambda can reach millions.
  inline FP_PRECISION Poisson(FP_PRECISION lambda) {
    if (!(lambda > 0.0)) return 0.0;

    constexpr FP_PRECISION kExactThreshold = 30.0;
    if (lambda < kExactThreshold) {
      const FP_PRECISION limit = std::exp(-lambda);
      FP_PRECISION product = NextDouble();
      int count = 0;
      while (product > limit) {
        count++;
        product *= NextDouble();
      }
      return static_cast<FP_PRECISION>(count);
    }

    // Round so the result stays a whole number of electrons, and clamp: the
    // Gaussian tail can go negative where the Poisson cannot.
    const FP_PRECISION approx = Gaussian(lambda, std::sqrt(lambda));
    return std::max(static_cast<FP_PRECISION>(0.0), std::floor(approx + 0.5));
  }
};

}  // namespace core
