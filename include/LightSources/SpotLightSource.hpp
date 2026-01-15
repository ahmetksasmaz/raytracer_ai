#pragma once

#include "BaseLightSource.hpp"

class SpotLightSource : public BaseLightSource
{
public:
  SpotLightSource(const Vec3f &position, const Vec3f &direction, const Vec3f &intensity, FP_PRECISION coverage_angle, FP_PRECISION falloff_angle)
      : BaseLightSource(intensity), direction_(direction), position_(position), coverage_angle_(coverage_angle), falloff_angle_(falloff_angle) {}

  const Vec3f direction_;
  const Vec3f position_;
  const FP_PRECISION coverage_angle_;
  const FP_PRECISION falloff_angle_;
};