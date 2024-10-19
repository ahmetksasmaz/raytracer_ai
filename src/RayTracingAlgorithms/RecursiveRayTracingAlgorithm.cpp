#include <limits>

#include "Scene.hpp"

Vec3f Scene::RecursiveRayTracingAlgorithm(
    const Ray& ray, const std::shared_ptr<BaseObject> inside_object_ptr,
    int remaining_recursion, int max_recursion) {
  Vec3f pixel_value = {0, 0, 0};
  float t_hit = std::numeric_limits<float>::max();
  std::shared_ptr<BaseObject> hit_object_ptr = nullptr;

  if (inside_object_ptr == nullptr) {
    for (auto object : objects_) {
      float temp_hit = 0.0f;
      if (object->Intersect(ray, temp_hit)) {
        if (t_hit > temp_hit) {
          t_hit = temp_hit;
          hit_object_ptr = object;
        }
      }
    }
  } else {
    hit_object_ptr = inside_object_ptr;
    inside_object_ptr->Intersect(ray, t_hit, false);
  }

  if (hit_object_ptr) {
    if (inside_object_ptr) {
      Vec3f absorption_coefficient = dynamic_cast<DielectricMaterial*>(
                                         (inside_object_ptr->material_).get())
                                         ->absorption_coefficient_;
      pixel_value.x *= exp(-absorption_coefficient.x * t_hit);
      pixel_value.y *= exp(-absorption_coefficient.y * t_hit);
      pixel_value.z *= exp(-absorption_coefficient.z * t_hit);
    }
  } else {
    if (remaining_recursion == max_recursion) {
      pixel_value.x = background_color_.x;
      pixel_value.y = background_color_.y;
      pixel_value.z = background_color_.z;
    } else {
      pixel_value = {0, 0, 0};
    }
  }

  return pixel_value;
};