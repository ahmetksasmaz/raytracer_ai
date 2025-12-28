#pragma once

#include "BaseLightSource.hpp"
#include "../extern/parser.h"
#include "BaseImage.hpp"

class SphericalDirectionalLightSource : public BaseLightSource
{
public:
  SphericalDirectionalLightSource(RawEnvironmentMapType type, std::shared_ptr<BaseImage> image, RawEnvironmentMapSampler sampler)
      : BaseLightSource({0,0,0}), type_(type), image_(image), sampler_(sampler) {}

  Vec3f GetIntensity(const Vec3f& surface_normal, Vec3f& direction) const
  {
    // Map direction to texture coordinates
    FP_PRECISION u = 0.0f;
    FP_PRECISION v = 0.0f;

    FP_PRECISION theta;
    FP_PRECISION phi;
    FP_PRECISION pdf;
    
    if(sampler_ == kUniform){
      uniform_hemisphere_sample(theta, phi, pdf);
    }
    else if(sampler_ == kCosine){
      cosine_hemisphere_sample(theta, phi, pdf);
    }
    Vec3f tangent, bitangent;
    if (std::abs(surface_normal.x) > std::abs(surface_normal.y)) {
      tangent = normalize(cross(Vec3f{0, 1, 0}, surface_normal));
    } else {
      tangent = normalize(cross(Vec3f{1, 0, 0}, surface_normal));
    }
    bitangent = cross(surface_normal, tangent);
    direction = normalize(tangent * (std::sin(theta) * std::cos(phi)) +
                          bitangent * (std::sin(theta) * std::sin(phi)) +
                          surface_normal * std::cos(theta));

    if (type_ == kLatLong)
    {
      FP_PRECISION u = (1+atan2(direction.x, -direction.z)/M_PI)/2.0;
      FP_PRECISION v = acos(direction.y)/M_PI;
    }
    else if (type_ == kProbe)
    {
      FP_PRECISION r = (acos(-direction.z)) / (M_PI * sqrt(direction.x * direction.x + direction.y * direction.y));
      u = (r*direction.x + 1.0) / 2.0;
      v = (-r*direction.y + 1.0) / 2.0;
    }

    int x = static_cast<int>(u * image_->width_);
    int y = static_cast<int>(v * image_->height_);

    Vec3f color_value = (*image_)(x, y);

    return color_value / pdf;
  }

  const RawEnvironmentMapType type_;
  const std::shared_ptr<BaseImage> image_;
  const RawEnvironmentMapSampler sampler_;
};