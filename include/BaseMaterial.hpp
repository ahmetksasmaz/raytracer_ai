#pragma once

#include "../extern/parser.h"

using namespace parser;

class BaseMaterial {
 public:
  BaseMaterial(Vec3f ambient, Vec3f diffuse, Vec3f specular,
               float phong_exponent)
      : ambient_(ambient),
        diffuse_(diffuse),
        specular_(specular),
        phong_exponent_(phong_exponent) {}
  virtual ~BaseMaterial() {}

  Vec3f ambient_;
  Vec3f diffuse_;
  Vec3f specular_;
  float phong_exponent_;
};