#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"

class SphereObject : public BaseObject {
 public:
  SphereObject(std::shared_ptr<BaseMaterial> material, const Vec3f& center,
               const float radius, const Mat4x4f& transform_matrix)
      : BaseObject(material, transform_matrix),
        center_(center),
        radius_(radius) {};

  std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      const Ray& ray, float& t_hit, Vec3f& intersection_normal, bool,
      bool) const override;

  virtual ~SphereObject() = default;

  void Preprocess(bool high_level_bvh_enabled,
                  bool low_level_bvh_enabled) override;

 private:
  const float radius_;
  const Vec3f center_;
};