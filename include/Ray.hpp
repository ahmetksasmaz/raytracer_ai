#pragma once
#include "../extern/parser.h"

using namespace parser;

class Ray {
 public:
  Ray(const Vec2i& pixel, const Vec3f& origin, const Vec3f& direction)
      : pixel_(pixel), origin_(origin), direction_(direction) {}
  const Vec2i pixel_;
  const Vec3f origin_;
  const Vec3f direction_;
  ~Ray() {}
};