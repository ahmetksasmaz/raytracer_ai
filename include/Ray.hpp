#pragma once
#include "../extern/parser.h"
#include "Helper.hpp"

using namespace parser;

class Ray {
 public:
  Ray(const Vec2i& pixel, const Vec3f origin, Vec3f direction,
      Vec2f diff = {0.0, 0.0}, float time = 0.0)
      : pixel_(pixel),
        origin_(origin),
        direction_(direction),
        diff_(diff),
        time_(time) {}
  const Vec2i pixel_;
  const Vec3f origin_;
  Vec3f direction_;
  const Vec2f diff_;
  const float time_;
  ~Ray() {}
};