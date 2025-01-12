#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class ConductorMaterial : public BaseMaterial {
 public:
  ConductorMaterial(const std::shared_ptr<BaseSpectrum> ambient,
                    const std::shared_ptr<BaseSpectrum> diffuse,
                    const std::shared_ptr<BaseSpectrum> specular,
                    float phong_exponent, float roughness,
                    const std::shared_ptr<BaseSpectrum> mirror,
                    float refraction_index, float absorption_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror),
        refraction_index_(refraction_index),
        absorption_index_(absorption_index) {}

  const std::shared_ptr<BaseSpectrum> mirror_;
  const float refraction_index_;
  const float absorption_index_;
};