#include "MeshInstanceObject.hpp"

#include <cstring>
#include <iostream>
#include <limits>

MeshInstanceObject::MeshInstanceObject(std::shared_ptr<BaseMaterial> material, std::vector<std::shared_ptr<BaseTextureMap>> textures,
                                       std::shared_ptr<MeshObject> mesh_object,
                                       const Vec3f motion_blur,
                                       const Mat4x4f& transform_matrix,
                                       RawScalingFlip scaling_flip)
    : BaseObject(material ? material : mesh_object->material_, textures, motion_blur,
                 transform_matrix, scaling_flip),
      mesh_object_(mesh_object) {};

bool MeshInstanceObject::Intersect(
    const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool backface_culling,
    bool stop_at_any_hit) const {
  bool hit = false;

  Vec3f temp_intersection_normal;

  Vec3f transformed_ray_origin =
      inverse_transform_matrix_ * (ray.origin_ - motion_blur_ * ray.time_);
  Vec3f transformed_ray_direction =
      normalize(inverse_transform_matrix_ ^ ray.direction_);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction, ray.diff_, ray.time_};

  FP_PRECISION mesh_hit = std::numeric_limits<FP_PRECISION>::max();
  
  int hit_index = mesh_object_->triangle_bvh_.Intersect(transformed_ray, mesh_object_->triangle_objects_,
                                        mesh_hit, temp_intersection_normal, tex_coords,
                                        hit_u_vector, hit_v_vector, tangent_vector, bitangent_vector,
                                        backface_culling, stop_at_any_hit);
  if (hit_index >= 0) {
    hit = true;
  }
  
  if (hit) {
    Vec3f local_point =
        transformed_ray.origin_ + mesh_hit * transformed_ray.direction_;
    Vec3f global_point = transform_matrix_ * local_point + motion_blur_ * ray.time_;
    Vec3f diff = global_point - ray.origin_;
    t_hit = norm(diff);
    intersection_normal = normalize(transform_matrix_ ^ temp_intersection_normal);
  }

  return hit;
}

void MeshInstanceObject::Preprocess(bool) {
    // Start from the base mesh's OBJECT-space bounds. The previous code took the
    // base's world-space min/max and pushed just those two points through the
    // base's inverse transform -- but corners of an axis-aligned box do not map
    // to corners of the inverse box under rotation, so the recovered bounds were
    // wrong and parts of a rotated instance were culled away.
    Vec3f min_point = mesh_object_->local_min_point_;
    Vec3f max_point = mesh_object_->local_max_point_;

    FP_PRECISION x_min = min_point.x;
    FP_PRECISION y_min = min_point.y;
    FP_PRECISION z_min = min_point.z;
    FP_PRECISION x_max = max_point.x;
    FP_PRECISION y_max = max_point.y;
    FP_PRECISION z_max = max_point.z;

    Vec3f p0 = Vec3f{x_min, y_min, z_min};
    Vec3f p1 = Vec3f{x_max, y_min, z_min};
    Vec3f p2 = Vec3f{x_min, y_max, z_min};
    Vec3f p3 = Vec3f{x_max, y_max, z_min};
    Vec3f p4 = Vec3f{x_min, y_min, z_max};
    Vec3f p5 = Vec3f{x_max, y_min, z_max};
    Vec3f p6 = Vec3f{x_min, y_max, z_max};
    Vec3f p7 = Vec3f{x_max, y_max, z_max};

    p0 = transform_matrix_ * p0;
    p1 = transform_matrix_ * p1;
    p2 = transform_matrix_ * p2;
    p3 = transform_matrix_ * p3;
    p4 = transform_matrix_ * p4;
    p5 = transform_matrix_ * p5;
    p6 = transform_matrix_ * p6;
    p7 = transform_matrix_ * p7;

    Vec3f p0_motion = p0 + motion_blur_;
    Vec3f p1_motion = p1 + motion_blur_;
    Vec3f p2_motion = p2 + motion_blur_;
    Vec3f p3_motion = p3 + motion_blur_;
    Vec3f p4_motion = p4 + motion_blur_;
    Vec3f p5_motion = p5 + motion_blur_;
    Vec3f p6_motion = p6 + motion_blur_;
    Vec3f p7_motion = p7 + motion_blur_;

    min_point = bounding_volume_min({p0, p1, p2, p3, p4, p5, p6, p7, p0_motion,
                                     p1_motion, p2_motion, p3_motion, p4_motion,
                                     p5_motion, p6_motion, p7_motion});
    max_point = bounding_volume_max({p0, p1, p2, p3, p4, p5, p6, p7, p0_motion,
                                     p1_motion, p2_motion, p3_motion, p4_motion,
                                     p5_motion, p6_motion, p7_motion});

    InitializeSelf(min_point, max_point);
}