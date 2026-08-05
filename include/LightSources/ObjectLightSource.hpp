#pragma once

#include "BaseLightSource.hpp"

class ObjectLightSource : public BaseLightSource
{
public:
  ObjectLightSource( const Vec3f &radiance)
      : BaseLightSource(Vec3f{0, 0, 0}), radiance_(radiance) {}

  // Draws a point on the emitter as seen from intersection_point and returns the
  // SOLID-ANGLE pdf of having drawn it. Implementations must obtain that pdf by
  // calling PdfSolidAngle below, never by recomputing it inline -- see there.
  virtual void Sample(const Vec3f& intersection_point, Vec3f &sample_point, Vec3f& sample_normal, FP_PRECISION &pdf) const = 0;

  // Solid-angle pdf that Sample() WOULD have returned for a given point on this
  // emitter. Multiple importance sampling needs exactly this: when a BSDF ray
  // happens to land on a light, the balance heuristic has to know how likely the
  // light-sampling strategy was to pick that same direction. Both pdfs in an MIS
  // weight must describe the SAME direction; drawing a fresh random sample to
  // get the second one produces a pdf for an unrelated direction and the weights
  // become meaningless.
  // Returns 0 for directions this emitter cannot produce (e.g. its back face).
  virtual FP_PRECISION PdfSolidAngle(const Vec3f& reference_point,
                                     const Vec3f& light_point,
                                     const Vec3f& light_normal) const = 0;

  const Vec3f radiance_;
  FP_PRECISION total_area_;
};