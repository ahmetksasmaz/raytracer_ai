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

    virtual void GetGradientAt(Vec2f tex_coords, Vec3f space_coords, Vec2f hit_u_vector, Vec2f hit_v_vector, Vec3f hit_tangent_vector, Vec3f hit_bitangent_vector, Vec3f &gradient_u, Vec3f &gradient_v) const override{
      FP_PRECISION delta_u = 1.0 / image_->width_;
      FP_PRECISION delta_v = 1.0 / image_->height_;
      
      Vec2f tex_coords_u1 = tex_coords + delta_u * hit_u_vector;
      Vec2f tex_coords_u2 = tex_coords - delta_u * hit_u_vector;
      Vec3f color_u1 = GetColorAt(tex_coords_u1, space_coords);
      Vec3f color_u2 = GetColorAt(tex_coords_u2, space_coords);
      
      Vec2f tex_coords_v1 = tex_coords + delta_v * hit_v_vector;
      Vec2f tex_coords_v2 = tex_coords - delta_v * hit_v_vector;
      Vec3f color_v1 = GetColorAt(tex_coords_v1, space_coords);
      Vec3f color_v2 = GetColorAt(tex_coords_v2, space_coords);
      
      Vec3f grad_vec_u = (color_u1 - color_u2) / (255.0 * delta_u);// / (2*delta_u);
      Vec3f grad_vec_v = (color_v1 - color_v2) / (255.0 * delta_v);// / (2*delta_v);
      FP_PRECISION grad_vec_u_gray = (grad_vec_u.x + grad_vec_u.y + grad_vec_u.z)/3.0;
      FP_PRECISION grad_vec_v_gray = (grad_vec_v.x + grad_vec_v.y + grad_vec_v.z)/3.0;
      gradient_u = Vec3f{grad_vec_u_gray, grad_vec_u_gray, grad_vec_u_gray};
      gradient_v = Vec3f{grad_vec_v_gray, grad_vec_v_gray, grad_vec_v_gray};
    }
private:
  const RawTextureMapInterpolationMode interpolation_mode_;
  const std::shared_ptr<BaseImage> image_;
};