#pragma once

#include "../extern/parser.h"

using namespace parser;

class BaseMaterial {
 public:
  BaseMaterial(const Vec3f& ambient, const Vec3f& diffuse,
               const Vec3f& specular, const float phong_exponent,
               const float roughness)
      : ambient_(ambient),
        diffuse_(diffuse),
        specular_(specular),
        phong_exponent_(phong_exponent),
        roughness_(roughness) {}
  virtual ~BaseMaterial() {}

  const Vec3f ambient_;
  const Vec3f diffuse_;
  const Vec3f specular_;
  const float phong_exponent_;
  const float roughness_;
};