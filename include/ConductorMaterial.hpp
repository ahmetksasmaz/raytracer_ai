#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class ConductorMaterial : public BaseMaterial {
 public:
  ConductorMaterial(const Vec3f& ambient, const Vec3f& diffuse,
                    const Vec3f& specular, FP_PRECISION phong_exponent,
                    FP_PRECISION roughness, const Vec3f& mirror,
                    FP_PRECISION refraction_index, FP_PRECISION absorption_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror),
        refraction_index_(refraction_index),
        absorption_index_(absorption_index) {}

  const Vec3f mirror_;
  const FP_PRECISION refraction_index_;
  const FP_PRECISION absorption_index_;
};