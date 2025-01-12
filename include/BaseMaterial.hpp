#pragma once

#include "../extern/parser.h"
#include "BaseSpectrum.hpp"

using namespace parser;

class BaseMaterial {
 public:
  BaseMaterial(const std::shared_ptr<BaseSpectrum> ambient,
               const std::shared_ptr<BaseSpectrum> diffuse,
               const std::shared_ptr<BaseSpectrum> specular,
               const float phong_exponent, const float roughness)
      : ambient_(ambient),
        diffuse_(diffuse),
        specular_(specular),
        phong_exponent_(phong_exponent),
        roughness_(roughness) {}
  virtual ~BaseMaterial() {}

  const std::shared_ptr<BaseSpectrum> ambient_;
  const std::shared_ptr<BaseSpectrum> diffuse_;
  const std::shared_ptr<BaseSpectrum> specular_;
  const float phong_exponent_;
  const float roughness_;
};