#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class ConductorMaterial : public BaseMaterial {
 public:
  ConductorMaterial(const Vec3f& ambient, const Vec3f& diffuse,
                    const Vec3f& specular, float phong_exponent,
                    float roughness, const Vec3f& mirror,
                    float refraction_index, float absorption_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror),
        refraction_index_(refraction_index),
        absorption_index_(absorption_index) {}

  const Vec3f mirror_;
  const float refraction_index_;
  const float absorption_index_;
};