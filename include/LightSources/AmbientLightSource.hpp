#pragma once

#include "BaseLightSource.hpp"

class AmbientLightSource : public BaseLightSource {
 public:
  AmbientLightSource(const Spectrum& intensity) : BaseLightSource(intensity) {}
};