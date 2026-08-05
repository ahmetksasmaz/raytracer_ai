#pragma once
#include "../extern/parser.h"
#include "Spectrum.hpp"
#include "BaseMaterial.hpp"

using namespace parser;

class DielectricMaterial : public BaseMaterial {
 public:
  DielectricMaterial(
    std::shared_ptr<BaseBRDF> brdf,
    const Spectrum& ambient, const Spectrum& diffuse,
                     const Spectrum& specular, const FP_PRECISION phong_exponent,
                     FP_PRECISION roughness, const Spectrum& mirror,
                     const Spectrum& absorption_coefficient,
                     const FP_PRECISION refraction_index)
      : BaseMaterial(brdf, ambient, diffuse, specular, phong_exponent, roughness, refraction_index),
        mirror_(mirror),
        absorption_coefficient_(absorption_coefficient){}

  const Spectrum mirror_;
  const Spectrum absorption_coefficient_;
};