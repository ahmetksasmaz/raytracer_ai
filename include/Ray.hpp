#pragma once
#include "../extern/parser.h"
#include "Helper.hpp"

using namespace parser;

class Ray {
 public:
  Ray(const Vec2i& pixel, const Vec3f origin, Vec3f direction)
      : pixel_(pixel), origin_(origin), direction_(direction) {}
  const Vec2i pixel_;
  const Vec3f origin_;
  Vec3f direction_;
  ~Ray() {}
};