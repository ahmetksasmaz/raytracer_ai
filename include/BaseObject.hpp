#pragma once

#include "../extern/parser.h"
#include "Ray.hpp"

using namespace parser;

class BaseObject {
 public:
  BaseObject(std::shared_ptr<BaseMaterial> material) : material_(material) {}

  virtual bool Intersect(const Ray& ray, float& t_hit) const = 0;

  const std::shared_ptr<BaseMaterial> material_;
  ~BaseObject() {}
};