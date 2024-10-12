#pragma once

#include "BaseLightSource.hpp"

class PointLightSource : public BaseLightSource {
 public:
  PointLightSource(Vec3f position, Vec3f intensity)
      : BaseLightSource(intensity), position_(position) {}

  Vec3f position_;
};