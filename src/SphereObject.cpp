#include "SphereObject.hpp"

std::shared_ptr<BoundingVolumeHierarchyElement> SphereObject::Intersect(
    const Ray& ray, float& t_hit, Vec3f& intersection_normal, bool,
    bool) const {
  Vec3f transformed_ray_origin = inverse_transform_matrix_ * ray.origin_;
  Vec3f transformed_ray_direction =
      normalize(inverse_transpose_transform_matrix_ * ray.direction_);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction};

  // Calculate the discriminant
  Vec3f oc = transformed_ray.origin_ - center_;
  float a = dot(transformed_ray.direction_, transformed_ray.direction_);
  float b = 2.0f * dot(oc, transformed_ray.direction_);
  float c = dot(oc, oc) - radius_ * radius_;
  float discriminant = b * b - 4 * a * c;

  // Check if the ray intersects with the sphere
  if (discriminant > 0) {
    // Find the closest intersection point
    float t = (-b - sqrt(discriminant)) / (2.0f * a);
    if (t > 1e-5) {
      Vec3f local_point =
          transformed_ray.origin_ + t * transformed_ray.direction_;
      Vec3f global_point = transform_matrix_ * local_point;
      t_hit = norm(ray.origin_ - global_point);
      intersection_normal = normalize(local_point - center_);
      if (scaling_flip_.sx) {
        intersection_normal.x = -intersection_normal.x;
      }
      if (scaling_flip_.sy) {
        intersection_normal.y = -intersection_normal.y;
      }
      if (scaling_flip_.sz) {
        intersection_normal.z = -intersection_normal.z;
      }
      return std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
          std::const_pointer_cast<BaseObject>(this->shared_from_this()));
    }

    t = (-b + sqrt(discriminant)) / (2.0f * a);
    if (t > 1e-5) {
      Vec3f local_point =
          transformed_ray.origin_ + t * transformed_ray.direction_;
      Vec3f global_point = transform_matrix_ * local_point;
      t_hit = norm(ray.origin_ - global_point);
      intersection_normal = normalize(local_point - center_);
      intersection_normal =
          normalize(inverse_transpose_transform_matrix_ * intersection_normal);
      return std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
          std::const_pointer_cast<BaseObject>(this->shared_from_this()));
    }
  }

  return nullptr;
}

void SphereObject::Preprocess(bool high_level_bvh_enabled,
                              bool low_level_bvh_enabled, bool) {
  if (high_level_bvh_enabled) {
    float x_min = center_.x - radius_;
    float y_min = center_.y - radius_;
    float z_min = center_.z - radius_;
    float x_max = center_.x + radius_;
    float y_max = center_.y + radius_;
    float z_max = center_.z + radius_;

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

    Vec3f min_point = bounding_volume_min({p0, p1, p2, p3, p4, p5, p6, p7});
    Vec3f max_point = bounding_volume_max({p0, p1, p2, p3, p4, p5, p6, p7});

    InitializeSelf(min_point, max_point);
  }
}