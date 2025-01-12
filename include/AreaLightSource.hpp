#pragma once

#include "BaseLightSource.hpp"

class AreaLightSource : public BaseLightSource {
 public:
  AreaLightSource(const Vec3f &position, const Vec3f &normal, const float size,
                  const float power,
                  const std::shared_ptr<BaseSpectrum> spectrum)
      : BaseLightSource(power, spectrum),
        position_(position),
        normal_(normal),
        size_(size) {}

  const Vec3f position_;
  const Vec3f normal_;
  const float size_;
};