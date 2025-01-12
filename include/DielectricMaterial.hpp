#pragma once
#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class DielectricMaterial : public BaseMaterial {
 public:
  DielectricMaterial(const std::shared_ptr<BaseSpectrum> ambient,
                     const std::shared_ptr<BaseSpectrum> diffuse,
                     const std::shared_ptr<BaseSpectrum> specular,
                     const float phong_exponent, float roughness,
                     const std::shared_ptr<BaseSpectrum> mirror,
                     const std::shared_ptr<BaseSpectrum> absorption_coefficient,
                     const float refraction_index)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror),
        absorption_coefficient_(absorption_coefficient),
        refraction_index_(refraction_index) {}

  const std::shared_ptr<BaseSpectrum> mirror_;
  const std::shared_ptr<BaseSpectrum> absorption_coefficient_;
  const float refraction_index_;
};