#pragma once

#include "../extern/parser.h"
#include "BaseTextureMap.hpp"

using namespace parser;


class ImageTextureMap : public BaseTextureMap {
 public:
  ImageTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor, std::shared_ptr<BaseImage> image, RawTextureMapInterpolationMode interpolation_mode) : BaseTextureMap(decal_mode, bump_factor), interpolation_mode_(interpolation_mode) {}
  virtual ~ImageTextureMap() = default;
private:
  const RawTextureMapInterpolationMode interpolation_mode_;
  const std::shared_ptr<BaseImage> image_;
};