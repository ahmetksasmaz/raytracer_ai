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
  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords, Vec2f, Vec2f) const override{
    bool x = ((int) floor((space_coords.x + offset_) * scale_)) % 2;
    bool y = ((int) floor((space_coords.y + offset_) * scale_)) % 2;
    bool z = ((int) floor((space_coords.z + offset_) * scale_)) % 2;

    bool xorXY = x != y;
    if (xorXY != z){
      return black_color_;
    }
    else{
      return white_color_;
    }
  }

  virtual void GetGradientAt(Vec2f tex_coords, Vec3f space_coords, Vec2f hit_u_vector, Vec2f hit_v_vector, Vec3f hit_tangent_vector, Vec3f hit_bitangent_vector, Vec3f &gradient_u, Vec3f &gradient_v) const override{
    gradient_u = Vec3f{0.0, 0.0, 0.0};
    gradient_v = Vec3f{0.0, 0.0, 0.0};
  }
private:
  const FP_PRECISION scale_;
  const FP_PRECISION offset_;
  const Vec3f black_color_;
  const Vec3f white_color_;
};