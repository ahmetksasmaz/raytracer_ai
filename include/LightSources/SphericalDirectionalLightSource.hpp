#pragma once

#include "BaseLightSource.hpp"
#include "../extern/parser.h"
#include "BaseImage.hpp"

class SphericalDirectionalLightSource : public BaseLightSource
{
public:
  SphericalDirectionalLightSource(RawEnvironmentMapType type, std::shared_ptr<BaseImage> image, RawEnvironmentMapSampler sampler)
      : BaseLightSource({0,0,0}), type_(type), image_(image), sampler_(sampler) {}

  Vec3f GetIntensity(const Vec3f& surface_normal, Vec3f& direction, bool no_sample = false) const
  {
    FP_PRECISION u = 0.0f;
    FP_PRECISION v = 0.0f;

    FP_PRECISION pdf;
    if(!no_sample){

      FP_PRECISION theta;
      FP_PRECISION phi;
      
      if(sampler_ == kUniform){
        uniform_hemisphere_sample(theta, phi, pdf);
      }
      else if(sampler_ == kCosine){
        cosine_hemisphere_sample(theta, phi, pdf);
      }
      Vec3f normal_prime;
      normal_prime.x = surface_normal.x;
      normal_prime.y = surface_normal.y;
      normal_prime.z = surface_normal.z;
      int min_index = 0;
      FP_PRECISION min_value = surface_normal.x;
      if (surface_normal.y < min_value)
      {
        min_value = surface_normal.y;
        min_index = 1;
      }
      if (surface_normal.z < min_value)
      {
        min_value = surface_normal.z;
        min_index = 2;
      }
      switch (min_index)
      {
      case 0:
        normal_prime.x = 1.0f;
        break;
      case 1:
        normal_prime.y = 1.0f;
        break;
      case 2:
        normal_prime.z = 1.0f;
        break;
      }
      normal_prime = normalize(normal_prime);
      Vec3f tangent = normalize(cross(normal_prime, surface_normal));
      Vec3f bitangent = cross(surface_normal, tangent);

      direction = normalize(tangent * (std::sin(theta) * std::cos(phi)) +
        bitangent * (std::sin(theta) * std::sin(phi)) +
        surface_normal * std::cos(theta));
    }
    else{
      direction = surface_normal;
      pdf = 1/(2*M_PI);
    }

    if (type_ == kLatLong)
    {
      u = (1+atan2(direction.x, -direction.z)/M_PI)/2.0;
      v = acos(direction.y)/M_PI;
    }
    else if (type_ == kProbe)
    {
      // Straight down the +/-z axis the denominator vanishes and r is 0/0. That
      // direction maps to the centre of the probe image, so substitute it
      // directly rather than casting a NaN to an image index.
      const FP_PRECISION radial = sqrt(direction.x * direction.x + direction.y * direction.y);
      if (radial < 1e-9) {
        u = 0.5;
        v = 0.5;
      } else {
        FP_PRECISION r = acos(-direction.z) / (M_PI * radial);
        u = (r*direction.x + 1.0) / 2.0;
        v = (-r*direction.y + 1.0) / 2.0;
      }
    }

    int x = static_cast<int>(u * image_->width_);
    int y = static_cast<int>(v * image_->height_);

    Vec3f color_value = (*image_)(x, y);
    if (!std::isfinite(pdf) || pdf <= 1e-6) {
      return {0, 0, 0};
    }
    return color_value / pdf;
  }

  const RawEnvironmentMapType type_;
  const std::shared_ptr<BaseImage> image_;
  const RawEnvironmentMapSampler sampler_;
};