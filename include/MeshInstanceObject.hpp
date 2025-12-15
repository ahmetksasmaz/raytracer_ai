#pragma once

#include "BaseObject.hpp"
#include "BoundingVolumeHierarchy.hpp"
#include "MeshObject.hpp"

class MeshInstanceObject : public BaseObject {
 public:
  MeshInstanceObject(std::shared_ptr<BaseMaterial> material, 
                     std::vector<std::shared_ptr<BaseTextureMap>> textures,
                     std::shared_ptr<MeshObject> mesh_object, Vec3f motion_blur,
                     const Mat4x4f& transform_matrix,
                     RawScalingFlip scaling_flip);

  std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, 
      Vec2f& tex_coords, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool backface_culling = true,
      bool stop_at_any_hit = false) const override;

  virtual ~MeshInstanceObject() = default;

  void Preprocess(bool high_level_bvh_enabled, bool low_level_bvh_enabled,
                  bool transform_enabled = true) override;

 private:
  std::shared_ptr<MeshObject> mesh_object_;
};