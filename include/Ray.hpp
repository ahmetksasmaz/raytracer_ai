#pragma once
#include "../extern/parser.h"
#include "Helper.hpp"

using namespace parser;

class Ray {
 public:
  Ray(const Vec2i& pixel, const Vec3f origin, Vec3f direction,
      Vec2f diff = {0.0, 0.0}, FP_PRECISION time = 0.0, const Vec3f up = {0,0,1}, Vec3f direction_i = {0,0,0}, Vec3f direction_j = {0,0,0})
      : pixel_(pixel),
        origin_(origin),
        direction_(direction),
        diff_(diff),
        up_(up),
        time_(time),
        direction_i_(direction_i),
        direction_j_(direction_j),
        inverse_direction_({1.0 / direction.x, 1.0 / direction.y, 1.0 / direction.z}),
        sign_{direction.x < 0 ? 1 : 0, direction.y < 0 ? 1 : 0, direction.z < 0 ? 1 : 0} {}
  const Vec2i pixel_;
  const Vec3f origin_;
  Vec3f direction_;
  const Vec2f diff_;
  const Vec3f up_;
  const FP_PRECISION time_;
  const Vec3f direction_i_;
  const Vec3f direction_j_;
  Vec3f inverse_direction_;
  int sign_[3];
  ~Ray() {}
};