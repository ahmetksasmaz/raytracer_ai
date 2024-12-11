#pragma once

#include "../extern/parser.h"

using namespace parser;

class BaseTextureMap {
 public:
  BaseTextureMap(RawTextureMapDecalMode decal_mode) : decal_mode_(decal_mode) {}
  virtual ~BaseTextureMap() = default;

 protected:
  RawTextureMapDecalMode decal_mode_;
};