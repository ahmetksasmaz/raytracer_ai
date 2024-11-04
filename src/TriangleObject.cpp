#include "TriangleObject.hpp"

std::shared_ptr<BoundingVolumeHierarchyElement> TriangleObject::Intersect(
    Ray& ray, float& t_hit, Vec3f& intersection_normal,
    bool backface_culling, bool) const {
  Vec3f transformed_ray_origin = inverse_transform_matrix_ * ray.origin_;
  Vec3f transformed_ray_destination = inverse_transform_matrix_ * (ray.origin_ + ray.direction_);
  Vec3f transformed_ray_direction =
      normalize(transformed_ray_destination - transformed_ray_origin);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction};

  if (backface_culling && dot(transformed_ray.direction_, normal_) > 0) {
    return nullptr;
  }

  Vec3f edge1 = v1_ - v0_;
  Vec3f edge2 = v2_ - v0_;
  Vec3f ray_cross_e2 = cross(transformed_ray.direction_, edge2);
  float det = dot(edge1, ray_cross_e2);

  float inv_det = 1.0 / det;
  Vec3f s = transformed_ray.origin_ - v0_;
  float u = inv_det * dot(s, ray_cross_e2);

  if (u < 0 || u > 1) {
    return nullptr;
  }

  Vec3f s_cross_e1 = cross(s, edge1);
  float v = inv_det * dot(transformed_ray.direction_, s_cross_e1);

  if (v < 0 || u + v > 1) {
    return nullptr;
  }

  float t = inv_det * dot(edge2, s_cross_e1);

  if (t > 1e-5) {
    Vec3f local_point =
        transformed_ray.origin_ + t * transformed_ray.direction_;
    Vec3f local_point_destination = local_point + normal_;
    Vec3f global_point = transform_matrix_ * local_point;
    Vec3f global_point_destination = transform_matrix_ * local_point_destination;
    Vec3f diff = global_point - ray.origin_;
    t_hit = norm(diff);
    Vec3f normalized_diff = normalize(diff);
    ray.direction_.x = normalized_diff.x;
    ray.direction_.y = normalized_diff.y;
    ray.direction_.z = normalized_diff.z;
    intersection_normal =
        normalize(global_point_destination - global_point);
    return std::dynamic_pointer_cast<BoundingVolumeHierarchyElement>(
        std::const_pointer_cast<BaseObject>(this->shared_from_this()));
  } else {
    return nullptr;
  }
}

void TriangleObject::Preprocess(bool high_level_bvh_enabled,
                                bool low_level_bvh_enabled,
                                bool transform_enabled) {
  normal_ = normalize(cross(v1_ - v0_, v2_ - v0_));

  if (high_level_bvh_enabled || low_level_bvh_enabled) {
    float x_min = std::min({v0_.x, v1_.x, v2_.x});
    float y_min = std::min({v0_.y, v1_.y, v2_.y});
    float z_min = std::min({v0_.z, v1_.z, v2_.z});
    float x_max = std::max({v0_.x, v1_.x, v2_.x});
    float y_max = std::max({v0_.y, v1_.y, v2_.y});
    float z_max = std::max({v0_.z, v1_.z, v2_.z});

    Vec3f p0 = Vec3f{x_min, y_min, z_min};
    Vec3f p1 = Vec3f{x_max, y_min, z_min};
    Vec3f p2 = Vec3f{x_min, y_max, z_min};
    Vec3f p3 = Vec3f{x_max, y_max, z_min};
    Vec3f p4 = Vec3f{x_min, y_min, z_max};
    Vec3f p5 = Vec3f{x_max, y_min, z_max};
    Vec3f p6 = Vec3f{x_min, y_max, z_max};
    Vec3f p7 = Vec3f{x_max, y_max, z_max};

    if (transform_enabled) {
      p0 = transform_matrix_ * p0;
      p1 = transform_matrix_ * p1;
      p2 = transform_matrix_ * p2;
      p3 = transform_matrix_ * p3;
      p4 = transform_matrix_ * p4;
      p5 = transform_matrix_ * p5;
      p6 = transform_matrix_ * p6;
      p7 = transform_matrix_ * p7;
    }

    Vec3f min_point = bounding_volume_min({p0, p1, p2, p3, p4, p5, p6, p7});
    Vec3f max_point = bounding_volume_max({p0, p1, p2, p3, p4, p5, p6, p7});

    InitializeSelf(min_point, max_point);
  }
}