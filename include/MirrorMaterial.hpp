#pragma once

#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class MirrorMaterial : public BaseMaterial {
 public:
  MirrorMaterial(Vec3f ambient, Vec3f diffuse, Vec3f specular,
                 float phong_exponent, Vec3f mirror)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent),
        mirror_(mirror) {}

  Vec3f mirror_;
};