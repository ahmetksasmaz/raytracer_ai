#pragma once

#include "../extern/parser.h"
#include "BaseMaterial.hpp"

using namespace parser;

class MirrorMaterial : public BaseMaterial {
 public:
  MirrorMaterial(const std::shared_ptr<BaseSpectrum> ambient,
                 const std::shared_ptr<BaseSpectrum> diffuse,
                 const std::shared_ptr<BaseSpectrum> specular,
                 float phong_exponent, float roughness,
                 const std::shared_ptr<BaseSpectrum> mirror)
      : BaseMaterial(ambient, diffuse, specular, phong_exponent, roughness),
        mirror_(mirror) {}

  const std::shared_ptr<BaseSpectrum> mirror_;
};