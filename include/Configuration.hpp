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

enum class FilteringAlgorithm {
  kBox = 0,
  kGaussian = 1,
  kExtendedGaussian = 2,
  kBest = 2,
  kMax = 2
};

enum class ApertureType {
  kCircular = 0,
  kSquare = 1,
  kPoly3 = 2,
  kPoly5 = 3,
  kPoly6 = 4,
  kDefault = 0,
  kMax = 4
};