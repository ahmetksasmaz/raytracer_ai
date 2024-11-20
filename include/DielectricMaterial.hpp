#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class DielectricMaterial : public BaseMaterial {
 public:
  DielectricMaterial(const Vec3f& ambient, const Vec3f& diffuse,
                     const Vec3f& specular, const float phong_exponent,
                     float roughness, const Vec3f& mirror,
                     const Vec3f& absorption_coefficient,
                     const float refraction_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror),
        absorption_coefficient_(absorption_coefficient),
        refraction_index_(refraction_index) {}

  const Vec3f mirror_;
  const Vec3f absorption_coefficient_;
  const float refraction_index_;
};