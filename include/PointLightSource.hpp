#pragma once

#include "BaseLightSource.hpp"

class PointLightSource : public BaseLightSource {
 public:
  PointLightSource(const Vec3f& position, const Vec3f& intensity)
      : BaseLightSource(intensity), position_(position) {}

  const Vec3f position_;
};