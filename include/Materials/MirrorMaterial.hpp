#pragma once

#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class MirrorMaterial : public BaseMaterial {
 public:
  MirrorMaterial(
    std::shared_ptr<BaseBRDF> brdf,
    const Vec3f& ambient, const Vec3f& diffuse,
                 const Vec3f& specular, FP_PRECISION phong_exponent, FP_PRECISION roughness,
                 const Vec3f& mirror, FP_PRECISION refraction_index, FP_PRECISION absorption_index)
      : BaseMaterial(brdf, ambient, diffuse, specular, phong_exponent, roughness, refraction_index, absorption_index),
        mirror_(mirror) {}

  const Vec3f mirror_;
};