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

    virtual void GetGradientAt(Vec2f tex_coords, Vec3f space_coords, Vec3f &gradient_u, Vec3f &gradient_v) const override{
      FP_PRECISION delta_u = 1.0 / (image_->width_);
      FP_PRECISION delta_v = 1.0 / (image_->height_);

      Vec3f color = GetColorAt(tex_coords, space_coords);
      FP_PRECISION gray_color = (color.x + color.y + color.z)/3.0;

      Vec2f tex_coords_u1 = Vec2f{tex_coords.x + delta_u, tex_coords.y};
      Vec3f color_u1 = GetColorAt(tex_coords_u1, space_coords);
      FP_PRECISION gray_color_u1 = (color_u1.x + color_u1.y + color_u1.z)/3.0;
      
      Vec2f tex_coords_v1 = Vec2f{tex_coords.x, tex_coords.y + delta_v};
      Vec3f color_v1 = GetColorAt(tex_coords_v1, space_coords);
      FP_PRECISION gray_color_v1 = (color_v1.x + color_v1.y + color_v1.z)/3.0;

      FP_PRECISION grad_vec_u = (gray_color_u1 - gray_color) / (delta_u);
      FP_PRECISION grad_vec_v = (gray_color_v1 - gray_color) / (delta_v);
      gradient_u = Vec3f{grad_vec_u, grad_vec_u, grad_vec_u};
      gradient_v = Vec3f{grad_vec_v, grad_vec_v, grad_vec_v};
    }
private:
  const RawTextureMapInterpolationMode interpolation_mode_;
  const std::shared_ptr<BaseImage> image_;
};