#pragma once

#include "../extern/parser.h"
#include "BaseTextureMap.hpp"

using namespace parser;

class PerlinTextureMap : public BaseTextureMap {
 public:
  PerlinTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor, const RawTextureMapNoiseConversionType noise_conversion,
const FP_PRECISION noise_scale,
const int num_octaves) : BaseTextureMap(decal_mode, bump_factor), noise_conversion_(noise_conversion),
noise_scale_(noise_scale), num_octaves_(num_octaves) {}
  virtual ~PerlinTextureMap() = default;
private:
  const RawTextureMapNoiseConversionType noise_conversion_;
  const FP_PRECISION noise_scale_;
  const int num_octaves_;
};