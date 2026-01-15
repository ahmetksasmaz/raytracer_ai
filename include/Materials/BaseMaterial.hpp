#pragma once

#include "../extern/parser.h"
#include "BaseBRDF.hpp"

using namespace parser;

class BaseMaterial {
 public:
  BaseMaterial(std::shared_ptr<BaseBRDF> brdf, const Vec3f& ambient, const Vec3f& diffuse,
               const Vec3f& specular, const FP_PRECISION phong_exponent,
               const FP_PRECISION roughness)
      : brdf_(brdf),
        ambient_(ambient),
        diffuse_(diffuse),
        specular_(specular),
        phong_exponent_(phong_exponent),
        roughness_(roughness) {}
  virtual ~BaseMaterial() {}

  const Vec3f ambient_;
  const Vec3f diffuse_;
  const Vec3f specular_;
  const FP_PRECISION phong_exponent_;
  const FP_PRECISION roughness_;
  std::shared_ptr<BaseBRDF> brdf_;
};