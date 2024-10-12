#pragma once
#include "../extern/parser.h"

using namespace parser;

class BaseRay {
 public:
  BaseRay(const Vec3f& origin, const Vec3f& direction, const unsigned int depth)
      : origin_(origin), direction_(direction), depth_(depth) {}
  const Vec3f origin_;
  const Vec3f direction_;
  const unsigned int depth_;
  virtual void FinishRay() = 0;
  ~BaseRay() {}
};