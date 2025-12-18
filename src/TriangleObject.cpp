#include "TriangleObject.hpp"

std::shared_ptr<BoundingVolumeHierarchyElement> TriangleObject::Intersect(
    Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool backface_culling,
    bool) const {
  Vec3f transformed_ray_origin =
      inverse_transform_matrix_ * (ray.origin_ - motion_blur_ * ray.time_);
  // Vec3f transformed_ray_destination =
  //     inverse_transform_matrix_ *
  //     (ray.origin_ - motion_blur_ * ray.time_ + ray.direction_);
  // Vec3f transformed_ray_direction =
  //     normalize(transformed_ray_destination - transformed_ray_origin);
  Vec3f transformed_ray_direction =
      normalize(inverse_transform_matrix_ ^ ray.direction_);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction, ray.diff_, ray.time_};

  if (backface_culling && dot(transformed_ray.direction_, normal_) > 0) {
    return nullptr;
  }

  Vec3f edge1 = v1_ - v0_;
  Vec3f edge2 = v2_ - v0_;
  Vec3f ray_cross_e2 = cross(transformed_ray.direction_, edge2);
  FP_PRECISION det = dot(edge1, ray_cross_e2);

  FP_PRECISION inv_det = 1.0 / det;
  Vec3f s = transformed_ray.origin_ - v0_;
  FP_PRECISION u = inv_det * dot(s, ray_cross_e2);

  if (u < 0 || u > 1) {
    return nullptr;
  }

  Vec3f s_cross_e1 = cross(s, edge1);
  FP_PRECISION v = inv_det * dot(transformed_ray.direction_, s_cross_e1);

  if (v < 0 || u + v > 1) {
    return nullptr;
  }

  FP_PRECISION t = inv_det * dot(edge2, s_cross_e1);

  if (t > 1e-5) {
    Vec3f local_point =
        transformed_ray.origin_ + t * transformed_ray.direction_;
    // Vec3f local_point_destination = local_point + normal_;
    Vec3f global_point = transform_matrix_ * local_point + motion_blur_ * ray.time_;
    // Vec3f global_point_destination =
    //     transform_matrix_ * local_point_destination + motion_blur_ * ray.time_;
    Vec3f diff = global_point - ray.origin_;
    t_hit = norm(diff);
    Vec3f normalized_diff = normalize(diff);
    ray.direction_.x = normalized_diff.x;
    ray.direction_.y = normalized_diff.y;
    ray.direction_.z = normalized_diff.z;
    intersection_normal = normalize(transform_matrix_ ^ normal_);

    // Calculate texture coordinates
    FP_PRECISION w = 1 - u - v;
    FP_PRECISION tex_u =
        tex_v0_.x * w + tex_v1_.x * u + tex_v2_.x * v;
    FP_PRECISION tex_v =
        tex_v0_.y * w + tex_v1_.y * u + tex_v2_.y * v;
    tex_coords = Vec2f{tex_u, tex_v};

    // Calculate hit u and v gradient vectors
    hit_u_vector = normalize(Vec2f{tex_v1_.x - tex_v0_.x, tex_v1_.y - tex_v0_.y});
    hit_v_vector = normalize(Vec2f{tex_v2_.x - tex_v0_.x, tex_v2_.y - tex_v0_.y});

    // Calculate tangent and bitangent vectors
    Vec3f delta_pos1 = edge1;
    Vec3f delta_pos2 = edge2;
    Vec2f delta_uv1 = Vec2f{tex_v1_.x - tex_v0_.x, tex_v1_.y - tex_v0_.y};
    Vec2f delta_uv2 = Vec2f{tex_v2_.x - tex_v0_.x, tex_v2_.y - tex_v0_.y};
    FP_PRECISION r = 1.0f / (delta_uv1.x * delta_uv2.y - delta_uv1.y * delta_uv2.x);
    tangent_vector = normalize((delta_pos1 * delta_uv2.y - delta_pos2 * delta_uv1.y) * r);
    bitangent_vector = normalize((delta_pos2 * delta_uv1.x - delta_pos1 * delta_uv2.x) * r);
    tangent_vector = normalize(transform_matrix_ ^ tangent_vector);
    bitangent_vector = normalize(transform_matrix_ ^ bitangent_vector);

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
    FP_PRECISION x_min = std::min({v0_.x, v1_.x, v2_.x});
    FP_PRECISION y_min = std::min({v0_.y, v1_.y, v2_.y});
    FP_PRECISION z_min = std::min({v0_.z, v1_.z, v2_.z});
    FP_PRECISION x_max = std::max({v0_.x, v1_.x, v2_.x});
    FP_PRECISION y_max = std::max({v0_.y, v1_.y, v2_.y});
    FP_PRECISION z_max = std::max({v0_.z, v1_.z, v2_.z});

    Vec3f p0 = Vec3f{x_min, y_min, z_min};
    Vec3f p1 = Vec3f{x_max, y_min, z_min};
    Vec3f p2 = Vec3f{x_min, y_max, z_min};
    Vec3f p3 = Vec3f{x_max, y_max, z_min};
    Vec3f p4 = Vec3f{x_min, y_min, z_max};
    Vec3f p5 = Vec3f{x_max, y_min, z_max};
    Vec3f p6 = Vec3f{x_min, y_max, z_max};
    Vec3f p7 = Vec3f{x_max, y_max, z_max};

    Vec3f p0_motion = p0;
    Vec3f p1_motion = p1;
    Vec3f p2_motion = p2;
    Vec3f p3_motion = p3;
    Vec3f p4_motion = p4;
    Vec3f p5_motion = p5;
    Vec3f p6_motion = p6;
    Vec3f p7_motion = p7;

    if (transform_enabled) {
      p0 = transform_matrix_ * p0;
      p1 = transform_matrix_ * p1;
      p2 = transform_matrix_ * p2;
      p3 = transform_matrix_ * p3;
      p4 = transform_matrix_ * p4;
      p5 = transform_matrix_ * p5;
      p6 = transform_matrix_ * p6;
      p7 = transform_matrix_ * p7;

      p0_motion = p0 + motion_blur_;
      p1_motion = p1 + motion_blur_;
      p2_motion = p2 + motion_blur_;
      p3_motion = p3 + motion_blur_;
      p4_motion = p4 + motion_blur_;
      p5_motion = p5 + motion_blur_;
      p6_motion = p6 + motion_blur_;
      p7_motion = p7 + motion_blur_;
    }

    Vec3f min_point = bounding_volume_min(
        {p0, p1, p2, p3, p4, p5, p6, p7, p0_motion, p1_motion, p2_motion,
         p3_motion, p4_motion, p5_motion, p6_motion, p7_motion});
    Vec3f max_point = bounding_volume_max(
        {p0, p1, p2, p3, p4, p5, p6, p7, p0_motion, p1_motion, p2_motion,
         p3_motion, p4_motion, p5_motion, p6_motion, p7_motion});

    InitializeSelf(min_point, max_point);
  }
}