#pragma once

enum class SamplingAlgorithm {
  kUniform = 0,
  kRandom = 1,
  kJittered = 2,
  kMultiJittered = 3,
  kHalton = 4,
  kHammersley = 5,
  kBest = 5,
  kMax = 5
};

// FilteringAlgorithm used to be here. Reconstruction is splat-based now --
// BaseCamera::SplatSample applies the Gaussian weight as each sample lands --
// so there is no filtering pass left to select between.

enum class ApertureType {
  kCircular = 0,
  kSquare = 1,
  kPoly3 = 2,
  kPoly5 = 3,
  kPoly6 = 4,
  kDefault = 0,
  kMax = 4
};