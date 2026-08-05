#pragma once

#include "BaseLightSource.hpp"

class AreaLightSource : public BaseLightSource
{
public:
  AreaLightSource(const Vec3f &position, const Spectrum &radiance, const Vec3f &normal, const FP_PRECISION size)
      : BaseLightSource(Spectrum()), position_(position), radiance_(radiance), normal_(normal), size_(size) {}

  const Vec3f position_;
  const Spectrum radiance_;
  const Vec3f normal_;
  const FP_PRECISION size_;
};