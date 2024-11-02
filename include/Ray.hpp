#pragma once
#include "../extern/parser.h"
#include "Helper.hpp"

using namespace parser;

class Ray {
 public:
  Ray(const Vec2i& pixel, const Vec3f& origin, const Vec3f& direction)
      : pixel_(pixel), origin_(origin), direction_(direction) {}
  Ray(Ray ray, Mat4x4f transform_matrix,
      Mat4x4f inverse_transpose_transform_matrix)
      : pixel_(ray.pixel_),
        origin_(transform_matrix * ray.origin_),
        direction_(inverse_transpose_transform_matrix * ray.direction_) {}
  const Vec2i pixel_;
  const Vec3f origin_;
  const Vec3f direction_;
  ~Ray() {}
};