#pragma once

#include "BaseLightSource.hpp"
#include "extern/parser.h"
#include "BaseImage.hpp"

class SphericalDirectionalLightSource : public BaseLightSource
{
public:
  SphericalDirectionalLightSource(RawEnvironmentMapType type, std::shared_ptr<BaseImage> image, RawEnvironmentMapSampler sampler)
      : BaseLightSource({0,0,0}), type_(type), image_(image), sampler_(sampler) {}

  const RawEnvironmentMapType type_;
  const std::shared_ptr<BaseImage> image_;
  const RawEnvironmentMapSampler sampler_;
};