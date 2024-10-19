#include "SphereObject.hpp"

bool SphereObject::Intersect(const Ray& ray, float& t_hit,
                             bool backface_culling) const {
  // Calculate the discriminant
  Vec3f oc = ray.origin_ - center_;
  float a = dot(ray.direction_, ray.direction_);
  float b = 2.0f * dot(oc, ray.direction_);
  float c = dot(oc, oc) - radius_ * radius_;
  float discriminant = b * b - 4 * a * c;

  // Check if the ray intersects with the sphere
  if (discriminant > 0) {
    // Find the closest intersection point
    float t = (-b - sqrt(discriminant)) / (2.0f * a);
    if (t > 1e-5) {
      t_hit = t;
      return true;
    }

    t = (-b + sqrt(discriminant)) / (2.0f * a);
    if (t > 1e-5) {
      t_hit = t;
      return true;
    }
  }

  return false;
}