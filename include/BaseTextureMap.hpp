#pragma once

#include "../extern/parser.h"

using namespace parser;

class BaseTextureMap {
 public:
  BaseTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor) : decal_mode_(decal_mode), bump_factor_(bump_factor) {}
  virtual ~BaseTextureMap() = default;
  FP_PRECISION GetDiffuseCoefficient() const {
    switch (decal_mode_) {
      case kReplaceKd:
      case kReplaceAll:
        return 1.0;
      case kBlendKd:
        return 0.5;
      default:
        return 0.0;
    }
  }
  FP_PRECISION GetSpecularCoefficient() const {
    switch (decal_mode_) {
      case kReplaceKs:
      case kReplaceAll:
        return 1.0;
      default:
        return 0.0;
    }
  }
  FP_PRECISION GetBackgroundCoefficient() const {
    switch (decal_mode_) {
      case kReplaceBackground:
        return 1.0;
      default:
        return 0.0;
    }
  }

  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords) const = 0;

 protected:
  RawTextureMapDecalMode decal_mode_;
  FP_PRECISION bump_factor_;
};