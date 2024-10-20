#include "TriangleObject.hpp"

bool TriangleObject::Intersect(const Ray& ray, float& t_hit,
                               Vec3f& intersection_normal,
                               bool backface_culling, bool) const {
  if (backface_culling && dot(ray.direction_, normal_) > 0) {
    return false;
  }

  Vec3f edge1 = v1_ - v0_;
  Vec3f edge2 = v2_ - v0_;
  Vec3f ray_cross_e2 = cross(ray.direction_, edge2);
  float det = dot(edge1, ray_cross_e2);

  if (det > -1e-5 && det < 1e-5) {
    return false;
  }

  float inv_det = 1.0 / det;
  Vec3f s = ray.origin_ - v0_;
  float u = inv_det * dot(s, ray_cross_e2);

  if (u < 0 || u > 1) {
    return false;
  }

  Vec3f s_cross_e1 = cross(s, edge1);
  float v = inv_det * dot(ray.direction_, s_cross_e1);

  if (v < 0 || u + v > 1) {
    return false;
  }

  float t = inv_det * dot(edge2, s_cross_e1);

  if (t > 1e-5) {
    t_hit = t;
    intersection_normal = normal_;
    return true;
  } else {
    return false;
  }
}

void TriangleObject::Preprocess() {
  normal_ = normalize(cross(v1_ - v0_, v2_ - v0_));
}