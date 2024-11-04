#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"

class TriangleObject : public BaseObject {
 public:
  TriangleObject(std::shared_ptr<BaseMaterial> material, const Vec3f& v0,
                 const Vec3f& v1, const Vec3f& v2,
                 const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip)
      : BaseObject(material, transform_matrix, scaling_flip),
        v0_(v0),
        v1_(v1),
        v2_(v2) {};

  std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      Ray& ray, float& t_hit, Vec3f& intersection_normal,
      bool backface_culling = true,
      bool stop_at_any_hit = false) const override;

  virtual ~TriangleObject() = default;
  void Preprocess(bool high_level_bvh_enabled, bool low_level_bvh_enabled,
                  bool transform_enabled = true) override;

 private:
  const Vec3f v0_;
  const Vec3f v1_;
  const Vec3f v2_;
  Vec3f normal_;
};