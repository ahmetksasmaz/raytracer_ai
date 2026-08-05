#pragma once

#include "BaseObject.hpp"
#include "Helper.hpp"

class PlaneObject : public BaseObject {
 public:
  PlaneObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
               const Vec3f& point, const Vec3f& normal, const Vec3f motion_blur,
               const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip)
      : BaseObject(material, textures, motion_blur, transform_matrix, scaling_flip),
        point_(transform_matrix * point),
        normal_(normalize(transform_matrix * (point + normal) - transform_matrix * point))
        {}

  bool Intersect(
      const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector,
      bool backface_culling = true, bool stop_at_any_hit = false) const override;

  bool IntersectPlane(
      const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, bool backface_culling = true, bool stop_at_any_hit = false) const;

  virtual ~PlaneObject() = default;

  void Preprocess(bool transform_enabled = true) override;

 private:
  const Vec3f point_;
  const Vec3f normal_;
};