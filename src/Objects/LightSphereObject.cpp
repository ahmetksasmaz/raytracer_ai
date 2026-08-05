#include "LightSphereObject.hpp"

#include <cmath>

bool LightSphereObject::Intersect(
    const Ray& ray, FP_PRECISION& t_hit, Vec3f& intersection_normal, Vec2f& tex_coords, Vec2f& hit_u_vector, Vec2f& hit_v_vector, Vec3f& tangent_vector, Vec3f& bitangent_vector, bool, bool) const {
  Vec3f transformed_ray_origin =
      inverse_transform_matrix_ * (ray.origin_ - motion_blur_ * ray.time_);
  // See SphereObject::Intersect -- directions use the inverse's linear part,
  // not the inverse-transpose (which is the normal transform).
  Vec3f transformed_ray_direction =
      normalize(inverse_transform_matrix_ ^ ray.direction_);
  Ray transformed_ray{ray.pixel_, transformed_ray_origin,
                      transformed_ray_direction, ray.diff_, ray.time_};

  Vec3f oc = transformed_ray.origin_ - center_;
  FP_PRECISION a = dot(transformed_ray.direction_, transformed_ray.direction_);
  FP_PRECISION b = 2.0f * dot(oc, transformed_ray.direction_);
  FP_PRECISION c = dot(oc, oc) - radius_ * radius_;
  FP_PRECISION discriminant = b * b - 4 * a * c;

  if (discriminant > 0) {
    FP_PRECISION t1 = (-b - sqrt(discriminant)) / (2.0f * a);
    FP_PRECISION t2 = (-b + sqrt(discriminant)) / (2.0f * a);
    FP_PRECISION t = t1 > 1e-5 ? t1 : t2;
    if (t > 1e-5) {
      Vec3f local_point =
          transformed_ray.origin_ + t * transformed_ray.direction_;
      Vec3f local_normal = normalize(local_point - center_);

      Vec3f approximated_normal = normalize(inverse_transpose_transform_matrix_ * local_normal);

      Vec3f global_point = transform_matrix_ * local_point + motion_blur_ * ray.time_;
      Vec3f diff = global_point - ray.origin_;
      t_hit = norm(diff);
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

        hit_u_vector = Vec2f{1/(2*M_PI*sin(t)), 0.0f};
        hit_v_vector = Vec2f{0.0f, 1/(M_PI)};

        return true;
    }
  }

  return false;
}

void LightSphereObject::Preprocess(bool) {
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

// Uniform sampling of the cone that this sphere subtends from the shading point.
//
// Done entirely in WORLD space. The previous version built the cone in object
// space and returned an object-space solid-angle pdf, then transformed the
// direction out to world space -- so under any scaling the pdf described a
// different cone than the one actually sampled.
void LightSphereObject::Sample(const Vec3f& intersection_point, Vec3f &sample_point, Vec3f& sample_normal, FP_PRECISION &pdf) const {
  pdf = 0.0;

  const Vec3f center = WorldCenter();
  const FP_PRECISION radius = WorldRadius();
  const Vec3f to_center = center - intersection_point;
  const FP_PRECISION distance = norm(to_center);

  // A shading point inside the emitter does not see a cone at all.
  if (distance <= radius || distance < 1e-9) return;

  const Vec3f w = to_center / distance;
  const FP_PRECISION sin_theta_max = radius / distance;
  const FP_PRECISION cos_theta_max =
      std::sqrt(std::max(static_cast<FP_PRECISION>(0.0),
                         1.0 - sin_theta_max * sin_theta_max));

  const FP_PRECISION cos_theta = 1.0 - FastRandom() * (1.0 - cos_theta_max);
  const FP_PRECISION sin_theta =
      std::sqrt(std::max(static_cast<FP_PRECISION>(0.0), 1.0 - cos_theta * cos_theta));
  const FP_PRECISION phi = 2.0 * M_PI * FastRandom();

  Vec3f u, v;
  BuildOrthonormalBasis(w, u, v);
  const Vec3f direction = FastNormalize(u * (sin_theta * std::cos(phi)) +
                                        v * (sin_theta * std::sin(phi)) +
                                        w * cos_theta);

  // Closest intersection of that direction with the sphere, solved directly.
  // The old code called Intersect() and ignored its return value, so a grazing
  // miss left t_hit at its sentinel and placed the sample point at ~1e308.
  const Vec3f oc = intersection_point - center;
  const FP_PRECISION b = dot(oc, direction);
  const FP_PRECISION c = dot(oc, oc) - radius * radius;
  const FP_PRECISION discriminant = std::max(static_cast<FP_PRECISION>(0.0), b * b - c);
  const FP_PRECISION t = -b - std::sqrt(discriminant);
  if (t <= 1e-9 || !std::isfinite(t)) return;

  sample_point = intersection_point + direction * t;
  sample_normal = FastNormalize(sample_point - center);
  pdf = PdfSolidAngle(intersection_point, sample_point, sample_normal);
}

FP_PRECISION LightSphereObject::PdfSolidAngle(const Vec3f& reference_point,
                                              const Vec3f&, const Vec3f&) const {
  // Uniform over the subtended cone, so the density depends only on the solid
  // angle of that cone -- the particular point on the sphere does not matter.
  const FP_PRECISION radius = WorldRadius();
  const FP_PRECISION distance = norm(WorldCenter() - reference_point);
  if (distance <= radius || distance < 1e-9) return 0.0;

  const FP_PRECISION sin_theta_max = radius / distance;
  const FP_PRECISION cos_theta_max =
      std::sqrt(std::max(static_cast<FP_PRECISION>(0.0),
                         1.0 - sin_theta_max * sin_theta_max));
  const FP_PRECISION solid_angle = 2.0 * M_PI * (1.0 - cos_theta_max);
  if (solid_angle <= 1e-12) return 0.0;

  const FP_PRECISION pdf = 1.0 / solid_angle;
  return std::isfinite(pdf) && pdf > 0.0 ? pdf : 0.0;
}
