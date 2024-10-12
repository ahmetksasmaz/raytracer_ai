#pragma once
#include "../extern/parser.h"

using namespace parser;

class Ray {
 public:
  Ray(const Vec3f& origin, const Vec3f& direction)
      : origin_(origin), direction_(direction) {}
  const Vec3f origin_;
  const Vec3f direction_;
  ~Ray() {}
};