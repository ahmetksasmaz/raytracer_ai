#pragma once
#include "../extern/parser.h"
#include "BaseRay.hpp"

using namespace parser;

class SubRay : public BaseRay {
 public:
  SubRay(const Vec3f& origin, const Vec3f& direction,
         std::shared_ptr<BaseRay> parent_ray)
      : BaseRay(origin, direction, parent_ray->depth_) {}
  ~SubRay() {}
};