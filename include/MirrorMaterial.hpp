#pragma once

#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class MirrorMaterial : public BaseMaterial {
 public:
  MirrorMaterial(const Vec3f& ambient, const Vec3f& diffuse,
                 const Vec3f& specular, float phong_exponent, float roughness,
                 const Vec3f& mirror)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror) {}

  const Vec3f mirror_;
};