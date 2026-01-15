#pragma once

#include "../extern/parser.h"
#include "BaseTextureMap.hpp"

using namespace parser;

class PerlinTextureMap : public BaseTextureMap {
 public:
  PerlinTextureMap(RawTextureMapDecalMode decal_mode, const FP_PRECISION bump_factor, const RawTextureMapNoiseConversionType noise_conversion,
const FP_PRECISION noise_scale,
const int num_octaves) : BaseTextureMap(decal_mode, bump_factor), noise_conversion_(noise_conversion),
noise_scale_(noise_scale), num_octaves_(num_octaves) {}
  virtual ~PerlinTextureMap() = default;
  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords, Vec2f, Vec2f) const override{
    FP_PRECISION s = 0.0;
    for (int i = 0; i < num_octaves_; i++){
      FP_PRECISION frequency = pow(2.0, i);
      FP_PRECISION amplitude = pow(0.5, i);
      s += amplitude *  perlin_noise(frequency * noise_scale_ * space_coords);
    }
    return Vec3f{s,s,s};
  }
  virtual void GetGradientAt(Vec2f tex_coords, Vec3f space_coords, Vec2f hit_u_vector, Vec2f hit_v_vector, Vec3f hit_tangent_vector, Vec3f hit_bitangent_vector, Vec3f &gradient_u, Vec3f &gradient_v) const override{
    FP_PRECISION delta = 1e-4;
    Vec3f point_x1 = space_coords + delta * normalize(hit_tangent_vector);
    Vec3f point_x0 = space_coords - delta * normalize(hit_tangent_vector);
    Vec3f point_y1 = space_coords + delta * normalize(hit_bitangent_vector);
    Vec3f point_y0 = space_coords - delta * normalize(hit_bitangent_vector);
    Vec3f color_x1 = GetColorAt(tex_coords, point_x1, Vec2f{}, Vec2f{});
    Vec3f color_x0 = GetColorAt(tex_coords, point_x0, Vec2f{}, Vec2f{});
    Vec3f color_y1 = GetColorAt(tex_coords, point_y1, Vec2f{}, Vec2f{});
    Vec3f color_y0 = GetColorAt(tex_coords, point_y0, Vec2f{}, Vec2f{});
    FP_PRECISION grad_u_x = (color_x1.x - color_x0.x) * (1.0 / (2.0 * delta));
    FP_PRECISION grad_u_y = (color_x1.y - color_x0.y) * (1.0 / (2.0 * delta));
    FP_PRECISION grad_u_z = (color_x1.z - color_x0.z) * (1.0 / (2.0 * delta));
    FP_PRECISION grad_v_x = (color_y1.x - color_y0.x) * (1.0 / (2.0 * delta));
    FP_PRECISION grad_v_y = (color_y1.y - color_y0.y) * (1.0 / (2.0 * delta));
    FP_PRECISION grad_v_z = (color_y1.z - color_y0.z) * (1.0 / (2.0 * delta));
    gradient_u = Vec3f{grad_u_x, grad_u_y, grad_u_z};
    gradient_v = Vec3f{grad_v_x, grad_v_y, grad_v_z};
  }
private:
  const RawTextureMapNoiseConversionType noise_conversion_;
  const FP_PRECISION noise_scale_;
  const int num_octaves_;

  FP_PRECISION perlin_noise(const Vec3f& p) const{
    Vec3f gradients[16] = {
      Vec3f{1, 1, 0},
      Vec3f{-1, 1, 0},
      Vec3f{1, -1, 0},
      Vec3f{-1, -1, 0},
      Vec3f{1, 0, 1},
      Vec3f{-1, 0, 1},
      Vec3f{1, 0, -1},
      Vec3f{-1, 0, -1},
      Vec3f{0, 1, 1},
      Vec3f{0, -1, 1},
      Vec3f{0, 1, -1},
      Vec3f{0, -1, -1},
      Vec3f{1, 1, 0},
      Vec3f{-1, 1, 0},
      Vec3f{0, -1, 1},
      Vec3f{0, -1, -1}
    };
    int table[16] = {
        15, 10, 5, 0, 14, 9, 4, 3, 13, 8, 7, 2, 12, 11, 6, 1
    };
    auto fade = [](FP_PRECISION t) {
      return t * t * t * (t * (t * -6 + 15) - 10) + 1;
    };

    int xi = (int)floor(p.x);
    int yi = (int)floor(p.y);
    int zi = (int)floor(p.z);

    FP_PRECISION n_prime = 0.0;
    for (int dx = 0; dx <= 1; dx++) {
      for (int dy = 0; dy <= 1; dy++) {
        for (int dz = 0; dz <= 1; dz++) {
          int idx;
          idx = table[abs(zi + dz) % 16];
          idx = table[abs(yi + dy + idx) % 16];
          idx = table[abs(xi + dx + idx) % 16];

          Vec3f distance_vector = Vec3f{p.x - (xi + dx), p.y - (yi + dy), p.z - (zi + dz)};
          n_prime += dot(gradients[idx], distance_vector) * fade(abs(distance_vector.x)) * fade(abs(distance_vector.y)) * fade(abs(distance_vector.z));
        }
      }
    }
    
    if(noise_conversion_ == RawTextureMapNoiseConversionType::kAbsVal) {
      n_prime = abs(n_prime);
    }else{
      n_prime = (n_prime + 1.0) / 2.0;
    }

    return n_prime;
  }
};