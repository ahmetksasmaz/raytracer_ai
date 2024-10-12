#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class DielectricMaterial : public BaseMaterial {
 public:
  DielectricMaterial(Vec3f ambient, Vec3f diffuse, Vec3f specular,
                     float phong_exponent, Vec3f mirror,
                     Vec3f absorption_coefficient, float refraction_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent),
        mirror_(mirror),
        absorption_coefficient_(absorption_coefficient),
        refraction_index_(refraction_index) {}

  Vec3f mirror_;
  Vec3f absorption_coefficient_;
  float refraction_index_;
};