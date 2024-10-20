#pragma once

#include <memory>

#include "../extern/parser.h"
#include "BaseMaterial.hpp"
#include "Ray.hpp"

using namespace parser;

class BaseObject {
 public:
  BaseObject(std::shared_ptr<BaseMaterial> material) : material_(material) {}

  virtual bool Intersect(const Ray& ray, float& t_hit,
                         Vec3f& intersection_normal,
                         bool backface_culling = true,
                         bool stop_at_any_hit = false) const = 0;

  std::shared_ptr<BaseMaterial> material_;

  virtual ~BaseObject() = default;
  virtual void Preprocess() {};
};