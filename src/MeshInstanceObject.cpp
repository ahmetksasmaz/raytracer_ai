#include "MeshInstanceObject.hpp"

#include <cstring>
#include <iostream>
#include <limits>

MeshInstanceObject::MeshInstanceObject(std::shared_ptr<BaseMaterial> material,
                                       std::shared_ptr<MeshObject> mesh_object,
                                       const Mat4x4f& transform_matrix,
                                       RawScalingFlip scaling_flip)
    : BaseObject(material ? material : mesh_object->material_, transform_matrix,
                 scaling_flip),
      mesh_object_(mesh_object) {};

std::shared_ptr<BoundingVolumeHierarchyElement> MeshInstanceObject::Intersect(
    const Ray& ray, float& t_hit, Vec3f& intersection_normal,
    bool backface_culling, bool stop_at_any_hit) const {
  bool hit = false;

  Vec3f transformed_ray_origin = inverse_transform_matrix_ * ray.origin_;
  Vec3f transformed_ray_direction =
      inverse_transpose_transform_matrix_ * ray.direction_;
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction};

  float mesh_hit = std::numeric_limits<float>::max();
  if (mesh_object_->left_) {
    if (mesh_object_->left_->Intersect(transformed_ray, mesh_hit,
                                       intersection_normal, backface_culling,
                                       stop_at_any_hit)) {
      hit = true;
    }
  } else {
    for (size_t i = 0; i < mesh_object_->triangle_objects_.size(); i++) {
      float temp_hit;
      Vec3f normal;
      if (!mesh_object_->triangle_objects_[i]->Intersect(
              transformed_ray, temp_hit, normal, backface_culling,
              stop_at_any_hit)) {
        continue;
      }

      if (temp_hit < mesh_hit) {
        mesh_hit = temp_hit;
        intersection_normal = normal;
      }
      hit = true;
      if (stop_at_any_hit) {
        break;
      }
    }
  }
  if (hit) {
    if (scaling_flip_.sx) {
      intersection_normal.x = -intersection_normal.x;
    }
    if (scaling_flip_.sy) {
      intersection_normal.y = -intersection_normal.y;
    }
    if (scaling_flip_.sz) {
      intersection_normal.z = -intersection_normal.z;
    }
    Vec3f local_point =
        transformed_ray.origin_ + mesh_hit * transformed_ray.direction_;
    Vec3f global_point = transform_matrix_ * local_point;
    t_hit = norm(ray.origin_ - global_point);
  }

  return hit ? std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
                   std::const_pointer_cast<BaseObject>(
                       this->shared_from_this()))
             : nullptr;
}

void MeshInstanceObject::Preprocess(bool high_level_bvh_enabled,
                                    bool low_level_bvh_enabled, bool) {
  if (high_level_bvh_enabled || low_level_bvh_enabled) {
    Vec3f min_point = mesh_object_->min_point_;
    Vec3f max_point = mesh_object_->max_point_;

    min_point = mesh_object_->inverse_transform_matrix_ * min_point;
    max_point = mesh_object_->inverse_transform_matrix_ * max_point;

    float x_min = min_point.x;
    float y_min = min_point.y;
    float z_min = min_point.z;
    float x_max = max_point.x;
    float y_max = max_point.y;
    float z_max = max_point.z;

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

    min_point = bounding_volume_min({p0, p1, p2, p3, p4, p5, p6, p7});
    max_point = bounding_volume_max({p0, p1, p2, p3, p4, p5, p6, p7});

    InitializeSelf(min_point, max_point);
  }
}