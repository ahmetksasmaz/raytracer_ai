#pragma once

#include "../extern/parser.h"

using namespace parser;

class BaseTextureMap {
 public:
  BaseTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor) : decal_mode_(decal_mode), bump_factor_(bump_factor) {}
  virtual ~BaseTextureMap() = default;
  bool GetReplaceAllFlag() const {
    return decal_mode_ == kReplaceAll;
  }

  FP_PRECISION GetDiffuseCoefficient() const {
    switch (decal_mode_) {
      case kReplaceKd:
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
        return 1.0;
      default:
        return 0.0;
    }
  }
  FP_PRECISION GetNormalCoefficient() const {
    switch (decal_mode_) {
      case kReplaceNormal:
        return 1.0;
      default:
        return 0.0;
    }
  }
  FP_PRECISION GetBumpCoefficient() const {
    switch (decal_mode_) {
      case kBumpNormal:
        return bump_factor_;
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

  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords, Vec2f tex_coords_di = {0,0}, Vec2f tex_coords_dj = {0,0}) const = 0;
  virtual void GetGradientAt(Vec2f tex_coords, Vec3f space_coords, Vec2f hit_u_vector, Vec2f hit_v_vector, Vec3f hit_tangent_vector, Vec3f hit_bitangent_vector, Vec3f &gradient_u, Vec3f &gradient_v) const = 0;

 protected:
  RawTextureMapDecalMode decal_mode_;
  FP_PRECISION bump_factor_;
};