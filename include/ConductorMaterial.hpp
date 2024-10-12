#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class ConductorMaterial : public BaseMaterial {
 public:
  ConductorMaterial(Vec3f ambient, Vec3f diffuse, Vec3f specular,
                    float phong_exponent, Vec3f mirror, float refraction_index,
                    float absorption_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent),
        mirror_(mirror),
        refraction_index_(refraction_index),
        absorption_index_(absorption_index) {}

  Vec3f mirror_;
  float refraction_index_;
  float absorption_index_;
};