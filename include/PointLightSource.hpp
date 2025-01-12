#pragma once

#include "BaseLightSource.hpp"

class PointLightSource : public BaseLightSource {
 public:
  PointLightSource(const Vec3f& position, const float power,
                   const std::shared_ptr<BaseSpectrum> spectrum)
      : BaseLightSource(power, spectrum), position_(position) {}

  const Vec3f position_;
};