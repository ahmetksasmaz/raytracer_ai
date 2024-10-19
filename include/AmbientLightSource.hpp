#pragma once

#include "BaseLightSource.hpp"

class AmbientLightSource : public BaseLightSource {
 public:
  AmbientLightSource(const Vec3f& intensity) : BaseLightSource(intensity) {}
};