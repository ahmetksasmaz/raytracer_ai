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
                 const Vec3f& mirror)
      : BaseMaterial(brdf, ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror) {}

  const Vec3f mirror_;
};