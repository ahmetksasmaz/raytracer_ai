#pragma once

#include "../extern/parser.h"
#include "Spectrum.hpp"
#include "BaseBRDF.hpp"

using namespace parser;

class BaseMaterial {
 public:
  BaseMaterial(std::shared_ptr<BaseBRDF> brdf, const Spectrum& ambient, const Spectrum& diffuse,
               const Spectrum& specular, const FP_PRECISION phong_exponent,
               const FP_PRECISION roughness, const FP_PRECISION refraction_index = -1.0f, const FP_PRECISION absorption_index = -1.0f)
      : brdf_(brdf),
        ambient_(ambient),
        diffuse_(diffuse),
        specular_(specular),
        phong_exponent_(phong_exponent),
        roughness_(roughness),
        refraction_index_(refraction_index),
        absorption_index_(absorption_index) {}
  virtual ~BaseMaterial() {}

  const Spectrum ambient_;
  const Spectrum diffuse_;
  const Spectrum specular_;
  const FP_PRECISION phong_exponent_;
  const FP_PRECISION roughness_;
  const FP_PRECISION refraction_index_;
  const FP_PRECISION absorption_index_;
  std::shared_ptr<BaseBRDF> brdf_;
};