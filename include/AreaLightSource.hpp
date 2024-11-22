#pragma once

#include "BaseLightSource.hpp"

class AreaLightSource : public BaseLightSource
{
public:
  AreaLightSource(const Vec3f &position, const Vec3f &radiance, const Vec3f &normal, const float size)
      : BaseLightSource(Vec3f{0, 0, 0}), position_(position), radiance_(radiance), normal_(normal), size_(size) {}

  const Vec3f position_;
  const Vec3f radiance_;
  const Vec3f normal_;
  const float size_;
};