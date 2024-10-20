#include <limits>

#include "Scene.hpp"

Vec3f Scene::RecursiveRayTracingAlgorithm(
    const Ray& ray, const std::shared_ptr<BaseObject> inside_object_ptr,
    int remaining_recursion, int max_recursion) {
  Vec3f pixel_value = {0, 0, 0};
  float t_hit = std::numeric_limits<float>::max();
  Vec3f hit_normal;
  std::shared_ptr<BaseObject> hit_object_ptr = nullptr;

  if (inside_object_ptr == nullptr) {
    for (auto object : objects_) {
      float temp_hit = std::numeric_limits<float>::max();
      Vec3f normal;
      if (object->Intersect(ray, temp_hit, normal)) {
        if (t_hit > temp_hit) {
          t_hit = temp_hit;
          hit_object_ptr = object;
          hit_normal = normal;
        }
      }
    }
  } else {
    hit_object_ptr = inside_object_ptr;
    inside_object_ptr->Intersect(ray, t_hit, hit_normal, false);
  }

  if (hit_object_ptr) {
#ifdef DEBUG
    std::cout << "\t\tHit object!" << std::endl;
#endif
#ifdef DEBUG
    std::cout << "\t\tHit normal : " << "(" << hit_normal.x << ","
              << hit_normal.y << "," << hit_normal.z << ")" << std::endl;
#endif
    // This is where the fun begins

    std::shared_ptr<BaseMaterial> material_ptr = hit_object_ptr->material_;

#ifdef DEBUG
    std::cout << "\t\tCalculating ambient." << std::endl;
#endif
    for (auto ambient_light : ambient_lights_) {
#ifdef DEBUG
      std::cout << "\t\tAmbient light intensity : " << "("
                << ambient_light->intensity_.x << ","
                << ambient_light->intensity_.y << ","
                << ambient_light->intensity_.z << ")" << std::endl;
#endif
      pixel_value +=
          hadamard(material_ptr->ambient_, ambient_light->intensity_);
#ifdef DEBUG
      std::cout << "\t\tPixel value after : " << "(" << pixel_value.x << ","
                << pixel_value.y << "," << pixel_value.z << ")" << std::endl;
#endif
    }

    Vec3f intersection_point =
        ray.origin_ + ray.direction_ * t_hit + hit_normal * shadow_ray_epsilon_;

#ifdef DEBUG
    int point_light_index = 0;
#endif

    for (auto point_light : point_lights_) {
#ifdef DEBUG
      std::cout << "\t\tCalculating point light : " << point_light_index
                << std::endl;
      std::cout << "\t\tLight position : " << "(" << point_light->position_.x
                << "," << point_light->position_.y << ","
                << point_light->position_.z << ")" << std::endl;
#endif
      Ray shadow_ray = {intersection_point,
                        normalize(point_light->position_ - intersection_point)};
      float distance_to_light =
          norm2(point_light->position_ - intersection_point);
      bool is_in_shadow = false;
#ifdef DEBUG
      int object_index = 0;
#endif
      for (auto object : objects_) {
        float shadow_hit = std::numeric_limits<float>::max();
        Vec3f shadow_normal;
        if (object->Intersect(shadow_ray, shadow_hit, shadow_normal, false)) {
          if (shadow_hit < sqrt(distance_to_light)) {
            is_in_shadow = true;
            break;
          }
        }
#ifdef DEBUG
        object_index++;
#endif
      }
      if (!is_in_shadow) {
#ifdef DEBUG
        std::cout << "\t\tI saw the light." << std::endl;
#endif
#ifdef DEBUG
        std::cout << "\t\tPoint light intensity : " << "("
                  << point_light->intensity_.x << ","
                  << point_light->intensity_.y << ","
                  << point_light->intensity_.z << ")" << std::endl;
#endif
#ifdef DEBUG
        std::cout << "\t\tDistance to light : " << sqrt(distance_to_light)
                  << std::endl;
#endif
        Vec3f light_direction =
            normalize(point_light->position_ - intersection_point);
        Vec3f half_vector = normalize(light_direction - ray.direction_);
        Vec3f diffuse_term =
            hadamard(material_ptr->diffuse_,
                     point_light->intensity_ / distance_to_light) *
            std::max(0.0f, dot(hit_normal, light_direction));
        Vec3f specular_term = {0, 0, 0};
        if (material_ptr->phong_exponent_ >= 0.0f) {
          specular_term =
              hadamard(material_ptr->specular_,
                       point_light->intensity_ / distance_to_light) *
              pow(std::max(0.0f, dot(hit_normal, half_vector)),
                  material_ptr->phong_exponent_);
        }
        pixel_value += diffuse_term + specular_term;
#ifdef DEBUG
        std::cout << "\t\tLight direction : " << "(" << light_direction.x << ","
                  << light_direction.y << "," << light_direction.z << ")"
                  << std::endl;
#endif
#ifdef DEBUG
        std::cout << "\t\tHalf vector : " << "(" << half_vector.x << ","
                  << half_vector.y << "," << half_vector.z << ")" << std::endl;
#endif
#ifdef DEBUG
        std::cout << "\t\tShadow direction vector : " << "("
                  << shadow_ray.direction_.x << "," << shadow_ray.direction_.y
                  << "," << shadow_ray.direction_.z << ")" << std::endl;
#endif
#ifdef DEBUG
        std::cout << "\t\tDiffuse - specular : " << "(" << diffuse_term.x << ","
                  << diffuse_term.y << "," << diffuse_term.z << ")" << ","
                  << "(" << diffuse_term.x << "," << diffuse_term.y << ","
                  << diffuse_term.z << ")" << std::endl;
#endif
#ifdef DEBUG
        std::cout << "\t\tPixel value after : " << "(" << pixel_value.x << ","
                  << pixel_value.y << "," << pixel_value.z << ")" << std::endl;
#endif
      }
#ifdef DEBUG
      point_light_index++;
#endif
    }

    if (remaining_recursion > 0) {
      MirrorMaterial* mirror_material_ptr =
          dynamic_cast<MirrorMaterial*>(material_ptr.get());
      ConductorMaterial* conductor_material_ptr =
          dynamic_cast<ConductorMaterial*>(material_ptr.get());
      DielectricMaterial* dielectric_material_ptr =
          dynamic_cast<DielectricMaterial*>(material_ptr.get());
      if (mirror_material_ptr) {
        Vec3f reflection_direction =
            ray.direction_ - 2 * dot(ray.direction_, hit_normal) * hit_normal;
        Ray reflection_ray = {
            intersection_point + shadow_ray_epsilon_ * hit_normal,
            reflection_direction};
        Vec3f reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion - 1,
            max_recursion);
        pixel_value += hadamard(reflection_color, mirror_material_ptr->mirror_);
      } else if (conductor_material_ptr) {
      } else if (dielectric_material_ptr) {
      }
    }

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