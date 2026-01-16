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

  std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool,
      bool) const override;

  virtual void Sample() const override = 0;

  virtual ~LightSphereObject() = default;

  void Preprocess(bool high_level_bvh_enabled, bool low_level_bvh_enabled,
                  bool transform_enabled = true) override;

 private:
  const FP_PRECISION radius_;
  const Vec3f center_;
};