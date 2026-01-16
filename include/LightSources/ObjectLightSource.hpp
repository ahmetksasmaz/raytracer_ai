#pragma once

#include "BaseLightSource.hpp"

class ObjectLightSource : public BaseLightSource
{
public:
  ObjectLightSource( const Vec3f &radiance)
      : BaseLightSource(Vec3f{0, 0, 0}), radiance_(radiance) {}

  virtual void Sample(const Vec3f& intersection_point, Vec3f &sample_point, Vec3f& sample_normal, FP_PRECISION &pdf) const = 0;

  const Vec3f radiance_;
};