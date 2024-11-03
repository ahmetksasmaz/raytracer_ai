#pragma once

#include "BaseObject.hpp"
#include "BoundingVolumeHierarchy.hpp"
#include "TriangleObject.hpp"

class MeshObject : public BaseObject {
 public:
  MeshObject(std::shared_ptr<BaseMaterial> material,
             const std::vector<RawFace>& raw_face_data,
             const std::vector<Vec3f>& raw_vertex_data,
             const Mat4x4f& transform_matrix, RawScalingFlip scaling_flip);
  MeshObject(std::shared_ptr<BaseMaterial> material,
             const std::string& ply_filename, const Mat4x4f& transform_matrix,
             RawScalingFlip scaling_flip);

  std::shared_ptr<BoundingVolumeHierarchyElement> Intersect(
      const Ray& ray, float& t_hit, Vec3f& intersection_normal,
      bool backface_culling = true,
      bool stop_at_any_hit = false) const override;

  virtual ~MeshObject() = default;

  void Preprocess(bool high_level_bvh_enabled, bool low_level_bvh_enabled,
                  bool transform_enabled = true) override;

  std::vector<std::shared_ptr<BoundingVolumeHierarchyElement>>
      triangle_objects_;
};