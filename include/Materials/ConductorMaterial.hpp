#pragma once
#include "../extern/parser.h"
#include "Spectrum.hpp"
#include "BaseMaterial.hpp"

using namespace parser;

class ConductorMaterial : public BaseMaterial {
 public:
  ConductorMaterial(std::shared_ptr<BaseBRDF> brdf, const Spectrum& ambient, const Spectrum& diffuse,
                    const Spectrum& specular, FP_PRECISION phong_exponent,
                    FP_PRECISION roughness, const Spectrum& mirror,
                    FP_PRECISION refraction_index, FP_PRECISION absorption_index)
      : BaseMaterial(brdf,ambient, diffuse, specular, phong_exponent, roughness, refraction_index, absorption_index),
        mirror_(mirror) {}

  const Spectrum mirror_;
};