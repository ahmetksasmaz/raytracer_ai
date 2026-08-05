#pragma once

#include "../extern/parser.h"
#include "BaseTextureMap.hpp"

using namespace parser;


class ImageTextureMap : public BaseTextureMap {
 public:
  ImageTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor, std::shared_ptr<BaseImage> image, RawTextureMapInterpolationMode interpolation_mode, FP_PRECISION normalizer, bool degamma = false) : BaseTextureMap(decal_mode, bump_factor), interpolation_mode_(interpolation_mode), image_(image), normalizer_(normalizer), degamma_(degamma) {}
  virtual ~ImageTextureMap() = default;
  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords, Vec2f tex_coords_di = {}, Vec2f tex_coords_dj = {}) const override{
    FP_PRECISION u = tex_coords.x - floor(tex_coords.x);
    FP_PRECISION v = tex_coords.y - floor(tex_coords.y);

    FP_PRECISION x = u * image_->width_;
    FP_PRECISION y = v * image_->height_;

    if (interpolation_mode_ == RawTextureMapInterpolationMode::kNearest){
      int xi = static_cast<int>(round(x));
      int yi = static_cast<int>(round(y));
      Vec3f color_uc = (*image_)(xi, yi) / normalizer_;
      return Decode(color_uc);
    }
    else if (interpolation_mode_ == RawTextureMapInterpolationMode::kBilinear){
      int x0 = static_cast<int>(floor(x));
      int x1 = x0 + 1;
      int y0 = static_cast<int>(floor(y));
      int y1 = y0 + 1;

      FP_PRECISION sx = x - x0;
      FP_PRECISION sy = y - y0;

      Vec3f c00 = (*image_)(x0, y0) / normalizer_;
      Vec3f c10 = (*image_)(x1, y0) / normalizer_;
      Vec3f c01 = (*image_)(x0, y1) / normalizer_;
      Vec3f c11 = (*image_)(x1, y1) / normalizer_;

      Vec3f c0 = (1 - sx) * c00 + sx * c10;
      Vec3f c1 = (1 - sx) * c01 + sx * c11;
      Vec3f c = (1 - sy) * c0 + sy * c1;

      return Decode(c);
    }
    else if (interpolation_mode_ == RawTextureMapInterpolationMode::kTrilinear){
      FP_PRECISION normi = norm2(tex_coords_di);
      FP_PRECISION normj = norm2(tex_coords_dj);
      FP_PRECISION level;
      if(normi >= normj) {
        Vec2f a2b2{tex_coords_di.x * (image_->width_), tex_coords_di.y * (image_->height_)};
        level = std::max(log2(norm(a2b2)), 0.0);
      } else {
        Vec2f a2b2{tex_coords_dj.x * (image_->width_), tex_coords_dj.y * (image_->height_)};
        level = std::max(log2(norm(a2b2)), 0.0);
      }
      // A zero-length differential gives log2(0) = -inf, and an unbounded level
      // would overflow the `1 << level` shifts below and index past the last
      // mip. Clamp to a level that actually exists, leaving one above it for
      // the interpolation partner.
      if (!std::isfinite(level)) level = 0.0;
      level = std::min(level, static_cast<FP_PRECISION>(std::max(image_->MaxLevel() - 1, 0)));

      Vec3f level_color;
      {
        int x0 = static_cast<int>(floor(x)) >> static_cast<int>(floor(level));
        int x1 = x0 + 1;
        int y0 = static_cast<int>(floor(y)) >> static_cast<int>(floor(level));
        int y1 = y0 + 1;

        FP_PRECISION sx = (x / (1 << static_cast<int>(floor(level)))) - x0;
        FP_PRECISION sy = (y / (1 << static_cast<int>(floor(level)))) - y0;

        Vec3f c00 = (*image_)(x0, y0, floor(level)) / normalizer_;
        Vec3f c10 = (*image_)(x1, y0, floor(level)) / normalizer_;
        Vec3f c01 = (*image_)(x0, y1, floor(level)) / normalizer_;
        Vec3f c11 = (*image_)(x1, y1, floor(level)) / normalizer_;

        Vec3f c0 = (1 - sx) * c00 + sx * c10;
        Vec3f c1 = (1 - sx) * c01 + sx * c11;
        level_color = (1 - sy) * c0 + sy * c1;
      }
      Vec3f next_level_color;
      {
        int x0 = static_cast<int>(floor(x)) >> static_cast<int>(floor(level) + 1);
        int x1 = x0 + 1;
        int y0 = static_cast<int>(floor(y)) >> static_cast<int>(floor(level) + 1);
        int y1 = y0 + 1;

        FP_PRECISION sx = (x / (1 << static_cast<int>(floor(level) + 1))) - x0;
        FP_PRECISION sy = (y / (1 << static_cast<int>(floor(level) + 1))) - y0;

        Vec3f c00 = (*image_)(x0, y0, floor(level) + 1) / normalizer_;
        Vec3f c10 = (*image_)(x1, y0, floor(level) + 1) / normalizer_;
        Vec3f c01 = (*image_)(x0, y1, floor(level) + 1) / normalizer_;
        Vec3f c11 = (*image_)(x1, y1, floor(level) + 1) / normalizer_;

        Vec3f c0 = (1 - sx) * c00 + sx * c10;
        Vec3f c1 = (1 - sx) * c01 + sx * c11;
        next_level_color = (1 - sy) * c0 + sy * c1;
      }
      level -= floor(level);
      return Decode(level_color * (1-level) + next_level_color * level);
    }
    // Unreachable for the modes above, but falling off the end of a
    // value-returning function is undefined behaviour, so be explicit.
    return Vec3f{0, 0, 0};
  }

    virtual void GetGradientAt(Vec2f tex_coords, Vec3f space_coords, Vec2f hit_u_vector, Vec2f hit_v_vector, Vec3f hit_tangent_vector, Vec3f hit_bitangent_vector, Vec3f &gradient_u, Vec3f &gradient_v) const override{
      
      Vec3f color = GetColorAt(tex_coords, space_coords);

      Vec2f tex_coords_u1 = tex_coords + hit_u_vector / norm(hit_tangent_vector);
      Vec3f color_u1 = GetColorAt(tex_coords_u1, space_coords);

      Vec2f tex_coords_v1 = tex_coords + hit_v_vector / norm(hit_bitangent_vector);
      Vec3f color_v1 = GetColorAt(tex_coords_v1, space_coords);

      gradient_u = (color_u1 - color) * norm(hit_tangent_vector) / norm(hit_u_vector);
      gradient_v = (color_v1 - color) * norm(hit_bitangent_vector) / norm(hit_v_vector);
    }
private:
  // sRGB -> linear. Texture values are otherwise used raw, so an sRGB-encoded
  // albedo map feeds gamma-encoded values into linear-space lighting.
  Vec3f Decode(const Vec3f& value) const {
    if (!degamma_) return value;
    return Vec3f{std::pow(std::max(value.x, static_cast<FP_PRECISION>(0.0)), 2.2),
                 std::pow(std::max(value.y, static_cast<FP_PRECISION>(0.0)), 2.2),
                 std::pow(std::max(value.z, static_cast<FP_PRECISION>(0.0)), 2.2)};
  }

  const RawTextureMapInterpolationMode interpolation_mode_;
  const std::shared_ptr<BaseImage> image_;
  const FP_PRECISION normalizer_;
  const bool degamma_;
};