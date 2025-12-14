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
  virtual Vec3f GetColorAt(Vec2f tex_coords, Vec3f space_coords) const override{
    FP_PRECISION s = 0.0;
    for (int i = 0; i < num_octaves_; i++){
      FP_PRECISION frequency = pow(2.0, i);
      FP_PRECISION amplitude = pow(0.5, i);
      s += amplitude * noise_scale_ * perlin_noise(frequency * space_coords);
    }
    return Vec3f{ 
      (noise_conversion_ == kAbsVal) ? abs(s) : (s + 1.0) / 2.0,
      (noise_conversion_ == kAbsVal) ? abs(s) : (s + 1.0) / 2.0,
      (noise_conversion_ == kAbsVal) ? abs(s) : (s + 1.0) / 2.0
    };
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
    auto fade = [](FP_PRECISION t) {
      return t * t * t * (t * (t * 6 - 15) + 10);
    };

    int xi = (int)floor(p.x);
    int yi = (int)floor(p.y);
    int zi = (int)floor(p.z);

    FP_PRECISION xf = p.x - xi;
    FP_PRECISION yf = p.y - yi;
    FP_PRECISION zf = p.z - zi;

    FP_PRECISION u = fade(xf);
    FP_PRECISION v = fade(yf);
    FP_PRECISION w = fade(zf);

    FP_PRECISION cumulative = 0.0;

    for(int i = 0; i < 2; i++){
      for(int j = 0; j < 2; j++){
        for(int k = 0; k < 2; k++){
          int idx;
          idx = (abs(zi + k) % 16);
          idx = (abs(yi + j + idx) % 16);
          idx = (abs(xi + i + idx) % 16);

          Vec3f gradient = gradients[idx];
          FP_PRECISION dot_product = gradient.x * (xf - i) +
                                    gradient.y * (yf - j) +
                                    gradient.z * (zf - k);
          cumulative += dot_product * 
                        (i ? u : (1 - u)) *
                        (j ? v : (1 - v)) *
                        (k ? w : (1 - w));
        }
      }
    }

    return cumulative;
  }
};