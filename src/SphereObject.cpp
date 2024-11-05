#include "SphereObject.hpp"

std::shared_ptr<BoundingVolumeHierarchyElement> SphereObject::Intersect(
    Ray& ray, float& t_hit, Vec3f& intersection_normal, bool,
    bool) const {
  Vec3f transformed_ray_origin = inverse_transform_matrix_ * ray.origin_;
  Vec3f transformed_ray_destination = inverse_transform_matrix_ * (ray.origin_ + ray.direction_);
  Vec3f transformed_ray_direction =
      normalize(transformed_ray_destination - transformed_ray_origin);
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
    float t1 = (-b - sqrt(discriminant)) / (2.0f * a);
    float t2 = (-b + sqrt(discriminant)) / (2.0f * a);
    float t = t1 > 1e-5 ? t1 : t2;
    if (t > 1e-5) {
      Vec3f local_point =
          transformed_ray.origin_ + t * transformed_ray.direction_;
      Vec3f local_normal = normalize(local_point - center_);

      float x = local_normal.x;
      float y = local_normal.y;
      float z = local_normal.z;
      float r = sqrt(x * x + y * y + z * z);
      float t = atan2(y, x);
      float p = acos(z / r);

      Vec3f local_normal_sample_1 = Vec3f{sin(p+0.05) * cos(t), sin(p+0.05) * sin(t), cos(p+0.05)};
      Vec3f local_normal_sample_2 = Vec3f{sin(p-0.05) * cos(t), sin(p-0.05) * sin(t), cos(p-0.05)};
      Vec3f local_normal_sample_3 = Vec3f{sin(p) * cos(t+0.05), sin(p) * sin(t+0.05), cos(p)};
      Vec3f local_normal_sample_4 = Vec3f{sin(p) * cos(t-0.05), sin(p) * sin(t-0.05), cos(p)};

      Vec3f local_point_sample_1 = center_ + radius_ * local_normal_sample_1;
      Vec3f local_point_sample_2 = center_ + radius_ * local_normal_sample_2;
      Vec3f local_point_sample_3 = center_ + radius_ * local_normal_sample_3;
      Vec3f local_point_sample_4 = center_ + radius_ * local_normal_sample_4;

      Vec3f global_point_sample_1 = transform_matrix_ * local_point_sample_1;
      Vec3f global_point_sample_2 = transform_matrix_ * local_point_sample_2;
      Vec3f global_point_sample_3 = transform_matrix_ * local_point_sample_3;
      Vec3f global_point_sample_4 = transform_matrix_ * local_point_sample_4;

      Vec3f first_axis_normal = normalize(global_point_sample_1 - global_point_sample_2);
      Vec3f second_axis_normal = normalize(global_point_sample_3 - global_point_sample_4);

      Vec3f approximated_normal = normalize(cross(first_axis_normal, second_axis_normal));

      Vec3f global_point = transform_matrix_ * local_point;
      Vec3f diff = global_point - ray.origin_;
      t_hit = norm(diff);
      Vec3f normalized_diff = normalize(diff);
      ray.direction_.x = normalized_diff.x;
      ray.direction_.y = normalized_diff.y;
      ray.direction_.z = normalized_diff.z;
      intersection_normal = approximated_normal;
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