#pragma once

#include "BaseLightSource.hpp"

class AmbientLightSource : public BaseLightSource {
 public:
  AmbientLightSource(const float power,
                     const std::shared_ptr<BaseSpectrum> spectrum)
      : BaseLightSource(power, spectrum) {}
};