#pragma once

#include "../extern/parser.h"

using namespace parser;

class BaseTextureMap {
 public:
  BaseTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor) : decal_mode_(decal_mode), bump_factor_(bump_factor) {}
  virtual ~BaseTextureMap() = default;

 protected:
  RawTextureMapDecalMode decal_mode_;
  FP_PRECISION bump_factor_;
};