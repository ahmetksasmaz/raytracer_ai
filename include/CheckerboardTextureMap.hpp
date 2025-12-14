#pragma once

#include "../extern/parser.h"
#include "BaseTextureMap.hpp"

using namespace parser;


class CheckerboardTextureMap : public BaseTextureMap {
 public:
  CheckerboardTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor, const FP_PRECISION scale, const FP_PRECISION offset,
const Vec3f black_color,
const Vec3f white_color) : BaseTextureMap(decal_mode, bump_factor), scale_(scale), offset_(offset),
black_color_(black_color), white_color_(white_color) {}
  virtual ~CheckerboardTextureMap() = default;
private:
  const FP_PRECISION scale_;
  const FP_PRECISION offset_;
  const Vec3f black_color_;
  const Vec3f white_color_;
};