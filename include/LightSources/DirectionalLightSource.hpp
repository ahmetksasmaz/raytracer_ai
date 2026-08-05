#pragma once

#include "BaseLightSource.hpp"

class DirectionalLightSource : public BaseLightSource
{
public:
  DirectionalLightSource(const Vec3f &direction, const Spectrum &radiance)
      : BaseLightSource(Spectrum()), direction_(direction), radiance_(radiance) {}

  const Vec3f direction_;
  const Spectrum radiance_;
};