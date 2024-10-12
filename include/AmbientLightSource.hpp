#pragma once

#include "BaseLightSource.hpp"

class AmbientLightSource : public BaseLightSource {
 public:
  AmbientLightSource(Vec3f position, Vec3f intensity)
      : BaseLightSource(intensity) {}
};