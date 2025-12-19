#include "SphereObject.hpp"

std::shared_ptr<BoundingVolumeHierarchyElement> SphereObject::Intersect(
    Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool, bool) const {
  Vec3f transformed_ray_origin =
      inverse_transform_matrix_ * (ray.origin_ - motion_blur_ * ray.time_);
    Vec3f transformed_ray_direction =
      normalize(inverse_transform_matrix_ ^ ray.direction_);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction, ray.diff_, ray.time_};

  // Calculate the discriminant
  Vec3f oc = transformed_ray.origin_ - center_;
  FP_PRECISION a = dot(transformed_ray.direction_, transformed_ray.direction_);
  FP_PRECISION b = 2.0f * dot(oc, transformed_ray.direction_);
  FP_PRECISION c = dot(oc, oc) - radius_ * radius_;
  FP_PRECISION discriminant = b * b - 4 * a * c;

  // Check if the ray intersects with the sphere
  if (discriminant > 0) {
    // Find the closest intersection point
    FP_PRECISION t1 = (-b - sqrt(discriminant)) / (2.0f * a);
    FP_PRECISION t2 = (-b + sqrt(discriminant)) / (2.0f * a);
    FP_PRECISION t = t1 > 1e-5 ? t1 : t2;
    if (t > 1e-5) {
      Vec3f local_point =
          transformed_ray.origin_ + t * transformed_ray.direction_;
      Vec3f local_normal = normalize(local_point - center_);
      Vec3f approximated_normal = normalize(transform_matrix_ ^ local_normal);

      Vec3f global_point = transform_matrix_ * local_point + motion_blur_ * ray.time_;
      Vec3f diff = global_point - ray.origin_;
      t_hit = norm(diff);
      Vec3f normalized_diff = normalize(diff);
      ray.direction_.x = normalized_diff.x;
      ray.direction_.y = normalized_diff.y;
      ray.direction_.z = normalized_diff.z;
      intersection_normal = approximated_normal;

      FP_PRECISION x = local_point.x - center_.x;
      FP_PRECISION y = local_point.y - center_.y;
      FP_PRECISION z = local_point.z - center_.z;
      FP_PRECISION r = sqrt(x * x + y * y + z * z);
      FP_PRECISION p = atan2(z, x);
      FP_PRECISION t = acos(y / r);

      // Compute texture coordinates
      FP_PRECISION u = (-p + M_PI) / (2.0 * M_PI);
      FP_PRECISION v = (t / M_PI);
      tex_coords = Vec2f{u, v};
      hit_u_vector = Vec2f{1.0f, 0.0f};
      hit_v_vector = Vec2f{0.0f, 1.0f};

      // Calculate TBN matrix
      Vec3f P_val = {r*sin(t)*cos(p), r*cos(t), r*sin(t)*sin(p)};
        Vec3f tangent;
        tangent.x = (2 * M_PI * P_val.z);
        tangent.y = 0;
        tangent.z = (-2 * M_PI * P_val.x);
        Vec3f bitangent;
        bitangent.x = (M_PI * P_val.y * cos(p));
        bitangent.y = (-M_PI * r * sin(t));
        bitangent.z = (M_PI * P_val.y * sin(p));
        tangent_vector = transform_matrix_ ^ tangent;
        bitangent_vector = transform_matrix_ ^ bitangent;
      return std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
          std::const_pointer_cast<BaseObject>(this->shared_from_this()));
    }
  }

  return nullptr;
}

void SphereObject::Preprocess(bool high_level_bvh_enabled,
                              bool low_level_bvh_enabled, bool) {
  if (high_level_bvh_enabled) {
    FP_PRECISION x_min = center_.x - radius_;
    FP_PRECISION y_min = center_.y - radius_;
    FP_PRECISION z_min = center_.z - radius_;
    FP_PRECISION x_max = center_.x + radius_;
    FP_PRECISION y_max = center_.y + radius_;
    FP_PRECISION z_max = center_.z + radius_;

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

    Vec3f min_point = bounding_volume_min(
        {p0, p1, p2, p3, p4, p5, p6, p7, p0_motion, p1_motion, p2_motion,
         p3_motion, p4_motion, p5_motion, p6_motion, p7_motion});
    Vec3f max_point = bounding_volume_max(
        {p0, p1, p2, p3, p4, p5, p6, p7, p0_motion, p1_motion, p2_motion,
         p3_motion, p4_motion, p5_motion, p6_motion, p7_motion});

    InitializeSelf(min_point, max_point);
  }
}