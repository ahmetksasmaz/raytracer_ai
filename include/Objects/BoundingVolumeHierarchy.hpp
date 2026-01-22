#pragma once

#include "../extern/parser.h"
#include "Ray.hpp"

using namespace parser;

struct BoundingBox {
  Vec3f min_point_;
  Vec3f max_point_;
  
  void Initialize(const Vec3f& min_point, const Vec3f& max_point) {
    min_point_ = min_point;
    max_point_ = max_point;
  }
};