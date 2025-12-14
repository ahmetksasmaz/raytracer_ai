#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"

class TriangleObject : public BaseObject {
 public:
  TriangleObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
                 const Vec3f& v0, const Vec3f& v1, const Vec3f& v2, const Vec2f& tex_v0, const Vec2f& tex_v1, const Vec2f& tex_v2, const Vec3f motion_blur,
                 const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip)
      : BaseObject(material, textures, motion_blur, transform_matrix, scaling_flip),
        v0_(v0),
        v1_(v1),
        v2_(v2),
        tex_v0_(tex_v0),
        tex_v1_(tex_v1),
        tex_v2_(tex_v2) {};

  std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords,
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
  const Vec2f tex_v0_;
  const Vec2f tex_v1_;
  const Vec2f tex_v2_;
};