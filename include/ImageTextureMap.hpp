#pragma once

#include "../extern/parser.h"
#include "BaseTextureMap.hpp"

using namespace parser;


class ImageTextureMap : public BaseTextureMap {
 public:
  ImageTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor, std::shared_ptr<BaseImage> image, RawTextureMapInterpolationMode interpolation_mode) : BaseTextureMap(decal_mode, bump_factor), interpolation_mode_(interpolation_mode), image_(image) {}
  virtual ~ImageTextureMap() = default;
  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords) const override{
    FP_PRECISION u = tex_coords.x - floor(tex_coords.x);
    FP_PRECISION v = tex_coords.y - floor(tex_coords.y);

    FP_PRECISION x = u * (image_->width_ - 1);
    FP_PRECISION y = v * (image_->height_ - 1);

    if (interpolation_mode_ == RawTextureMapInterpolationMode::kNearest){
      int xi = static_cast<int>(round(x));
      int yi = static_cast<int>(round(y));
      Vec3f color_uc = (*image_)(xi, yi);
      return Vec3f{color_uc.x, color_uc.y, color_uc.z};
    }
    else if (interpolation_mode_ == RawTextureMapInterpolationMode::kBilinear){
      int x0 = static_cast<int>(floor(x));
      int x1 = x0 + 1;
      int y0 = static_cast<int>(floor(y));
      int y1 = y0 + 1;

      FP_PRECISION sx = x - x0;
      FP_PRECISION sy = y - y0;

      Vec3f c00 = (*image_)(x0, y0);
      Vec3f c10 = (*image_)(x1, y0);
      Vec3f c01 = (*image_)(x0, y1);
      Vec3f c11 = (*image_)(x1, y1);

      Vec3f c0 = (1 - sx) * c00 + sx * c10;
      Vec3f c1 = (1 - sx) * c01 + sx * c11;
      Vec3f c = (1 - sy) * c0 + sy * c1;

      return c;
    }}
private:
  const RawTextureMapInterpolationMode interpolation_mode_;
  const std::shared_ptr<BaseImage> image_;
};