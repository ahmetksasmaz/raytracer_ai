#pragma once

#include "BaseLightSource.hpp"

class DirectionalLightSource : public BaseLightSource
{
public:
  DirectionalLightSource(const Vec3f &direction, const Vec3f &radiance)
      : BaseLightSource(Vec3f{0, 0, 0}), direction_(direction), radiance_(radiance) {}

  const Vec3f direction_;
  const Vec3f radiance_;
};