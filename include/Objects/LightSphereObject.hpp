#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"
#include "ObjectLightSource.hpp"

class LightSphereObject : public BaseObject, public ObjectLightSource {
 public:
  LightSphereObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
               const Vec3f& center, const FP_PRECISION radius, const Vec3f motion_blur,
               const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip, Vec3f radiance)
      : BaseObject(material, textures, motion_blur, transform_matrix, scaling_flip), ObjectLightSource(radiance),
        center_(center),
        radius_(radius) {};

  bool Intersect(
      const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool,
      bool) const override;

  virtual void Sample(const Vec3f& intersection_point, Vec3f &sample_point, Vec3f& sample_normal, FP_PRECISION &pdf) const override;

  virtual FP_PRECISION PdfSolidAngle(const Vec3f& reference_point,
                                     const Vec3f& light_point,
                                     const Vec3f& light_normal) const override;

  virtual ~LightSphereObject() = default;

  void Preprocess(bool transform_enabled = true) override;

 private:
  // Sampling happens in world space, so both the centre and the radius are
  // needed there. Uniform scaling is assumed: a non-uniformly scaled sphere is
  // an ellipsoid, and the subtended cone that this sampler is built on is not
  // the right shape for one.
  Vec3f WorldCenter() const { return transform_matrix_ * center_ + motion_blur_; }
  FP_PRECISION WorldRadius() const {
    return radius_ * norm(transform_matrix_ ^ Vec3f{1.0, 0.0, 0.0});
  }

  const FP_PRECISION radius_;
  const Vec3f center_;
};