#include <limits>

#include "Scene.hpp"

// #define RAY_DEBUG

#ifdef RAY_DEBUG

#include <fstream>

#endif

Vec3f Scene::RecursiveRayTracingAlgorithm(
    const Ray& ray,
    const std::shared_ptr<BoundingVolumeHierarchyElement> inside_object_ptr,
    int remaining_recursion, int max_recursion) {
  Vec3f pixel_value = {0, 0, 0};
  float t_hit = std::numeric_limits<float>::max();
  Vec3f hit_normal;
  std::shared_ptr<BoundingVolumeHierarchyElement> hit_object_ptr = nullptr;

  if (inside_object_ptr == nullptr) {
    if (configuration_.acceleration_.bvh_high_level_) {
      hit_object_ptr = bvh_root_->Intersect(ray, t_hit, hit_normal);
    } else {
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
    }

  } else {
    hit_object_ptr = inside_object_ptr;
    inside_object_ptr->Intersect(ray, t_hit, hit_normal, false);
    if (dot(ray.direction_, hit_normal) > 0) {
      hit_normal = -hit_normal;
    }
  }

#ifdef RAY_DEBUG

  std::ofstream output_file("rays/" + std::to_string(ray.pixel_.x) + "_" +
                                std::to_string(ray.pixel_.y) + ".txt",
                            std::ios::app);

  int current_recursion = max_recursion - remaining_recursion;

  output_file << current_recursion << "\t" << ray.origin_.x << "\t"
              << ray.origin_.y << "\t" << ray.origin_.z << "\t"
              << ray.direction_.x << "\t" << ray.direction_.y << "\t"
              << ray.direction_.z;

  if (hit_object_ptr) {
    output_file << "\t" << t_hit << "\t" << hit_normal.x << "\t" << hit_normal.y
                << "\t" << hit_normal.z;
  }

  output_file << std::endl;

  output_file.close();

#endif

  if (hit_object_ptr) {
    std::shared_ptr<BaseObject> hit_object_casted =
        std::dynamic_pointer_cast<BaseObject>(hit_object_ptr);

#ifdef DEBUG
    std::cout << "\t\tHit object!" << std::endl;
#endif
#ifdef DEBUG
    std::cout << "\t\tHit normal : " << "(" << hit_normal.x << ","
              << hit_normal.y << "," << hit_normal.z << ")" << std::endl;
#endif
    // This is where the fun begins

    std::shared_ptr<BaseMaterial> material_ptr = hit_object_casted->material_;

#ifdef DEBUG
    std::cout << "\t\tCalculating ambient." << std::endl;
#endif
    if (!inside_object_ptr) {
      if (configuration_.shading_.ambient_) {
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
                    << pixel_value.y << "," << pixel_value.z << ")"
                    << std::endl;
#endif
        }
      }
    }

    Vec3f intersection_point =
        ray.origin_ + ray.direction_ * t_hit + hit_normal * shadow_ray_epsilon_;

#ifdef DEBUG
    int point_light_index = 0;
#endif
    if (!inside_object_ptr) {
      for (auto point_light : point_lights_) {
#ifdef DEBUG
        std::cout << "\t\tCalculating point light : " << point_light_index
                  << std::endl;
        std::cout << "\t\tLight position : " << "(" << point_light->position_.x
                  << "," << point_light->position_.y << ","
                  << point_light->position_.z << ")" << std::endl;
#endif
        Ray shadow_ray = {
            ray.pixel_, intersection_point,
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

          if (configuration_.shading_.diffuse_) {
            Vec3f diffuse_term =
                hadamard(material_ptr->diffuse_,
                         point_light->intensity_ / distance_to_light) *
                std::max(0.0f, dot(hit_normal, light_direction));
            pixel_value += diffuse_term;
          }

          if (configuration_.shading_.specular_) {
            if (material_ptr->phong_exponent_ >= 0.0f) {
              Vec3f half_vector = normalize(light_direction - ray.direction_);
              Vec3f specular_term = {0, 0, 0};
              specular_term =
                  hadamard(material_ptr->specular_,
                           point_light->intensity_ / distance_to_light) *
                  pow(std::max(0.0f, dot(hit_normal, half_vector)),
                      material_ptr->phong_exponent_);
              pixel_value += specular_term;
            }
          }
#ifdef DEBUG
          std::cout << "\t\tLight direction : " << "(" << light_direction.x
                    << "," << light_direction.y << "," << light_direction.z
                    << ")" << std::endl;
#endif
#ifdef DEBUG
          std::cout << "\t\tHalf vector : " << "(" << half_vector.x << ","
                    << half_vector.y << "," << half_vector.z << ")"
                    << std::endl;
#endif
#ifdef DEBUG
          std::cout << "\t\tShadow direction vector : " << "("
                    << shadow_ray.direction_.x << "," << shadow_ray.direction_.y
                    << "," << shadow_ray.direction_.z << ")" << std::endl;
#endif
#ifdef DEBUG
          std::cout << "\t\tDiffuse - specular : " << "(" << diffuse_term.x
                    << "," << diffuse_term.y << "," << diffuse_term.z << ")"
                    << ","
                    << "(" << diffuse_term.x << "," << diffuse_term.y << ","
                    << diffuse_term.z << ")" << std::endl;
#endif
#ifdef DEBUG
          std::cout << "\t\tPixel value after : " << "(" << pixel_value.x << ","
                    << pixel_value.y << "," << pixel_value.z << ")"
                    << std::endl;
#endif
        }
#ifdef DEBUG
        point_light_index++;
#endif
      }
    }

    if (remaining_recursion > 0) {
      MirrorMaterial* mirror_material_ptr =
          dynamic_cast<MirrorMaterial*>(material_ptr.get());
      ConductorMaterial* conductor_material_ptr =
          dynamic_cast<ConductorMaterial*>(material_ptr.get());
      DielectricMaterial* dielectric_material_ptr =
          dynamic_cast<DielectricMaterial*>(material_ptr.get());
      if (mirror_material_ptr && configuration_.materials_.mirror_) {
        Vec3f reflection_direction =
            ray.direction_ - 2 * dot(ray.direction_, hit_normal) * hit_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction};
        Vec3f reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion - 1,
            max_recursion);
        pixel_value += hadamard(reflection_color, mirror_material_ptr->mirror_);
      } else if (conductor_material_ptr &&
                 configuration_.materials_.conductor_) {
        Vec3f reflection_direction =
            ray.direction_ - 2 * dot(ray.direction_, hit_normal) * hit_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction};
        Vec3f reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion - 1,
            max_recursion);

        float n2 = conductor_material_ptr->refraction_index_;
        float k2 = conductor_material_ptr->absorption_index_;
        float cos_theta = -dot(ray.direction_, hit_normal);
        float n2_k2_2 = n2 * n2 + k2 * k2;
        float n2_cos_theta_tw = 2 * n2 * cos_theta;
        float cos_theta_2 = cos_theta * cos_theta;
        float rs = (n2_k2_2 - n2_cos_theta_tw + cos_theta_2) /
                   (n2_k2_2 + n2_cos_theta_tw + cos_theta_2);
        float rp = (n2_k2_2 * cos_theta_2 - n2_cos_theta_tw + 1) /
                   (n2_k2_2 * cos_theta_2 + n2_cos_theta_tw + 1);
        float fresnel_reflection_ratio = (rs + rp) / 2;

        pixel_value +=
            hadamard(reflection_color, conductor_material_ptr->mirror_ *
                                           fresnel_reflection_ratio);
      } else if (dielectric_material_ptr &&
                 configuration_.materials_.dielectric_) {
        Vec3f reflection_color = {0, 0, 0};
        Vec3f reflection_direction =
            ray.direction_ - 2 * dot(ray.direction_, hit_normal) * hit_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction};
        reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion - 1,
            max_recursion);

        float n1 = inside_object_ptr
                       ? dielectric_material_ptr->refraction_index_
                       : 1.0f;
        float n2 = inside_object_ptr
                       ? 1.0
                       : dielectric_material_ptr->refraction_index_;

        float cos_theta = dot(-ray.direction_, hit_normal);
        float cos_phi_2 =
            1 - (n1 * n1 / (n2 * n2)) * (1 - cos_theta * cos_theta);
        if (cos_phi_2 > 0.0) {
          float cos_phi = sqrt(cos_phi_2);
          float r_p =
              (n1 * cos_theta - n2 * cos_phi) / (n1 * cos_theta + n2 * cos_phi);
          float r_s =
              (n1 * cos_phi - n2 * cos_theta) / (n1 * cos_phi + n2 * cos_theta);

          float fresnel_reflection_ratio = (r_p * r_p + r_s * r_s) / 2;
          float fresnel_transmission_ratio = 1.0 - fresnel_reflection_ratio;

          Vec3f refraction_direction =
              normalize((n1 / n2) * ray.direction_ +
                        (n1 / n2 * cos_theta - cos_phi) * hit_normal);
          Ray refraction_ray = {
              ray.pixel_,
              intersection_point - 2 * shadow_ray_epsilon_ * hit_normal,
              refraction_direction};
          // If the object type is triangle, inside_object_ptr is nullptr, check
          // later
          Vec3f refraction_color = RecursiveRayTracingAlgorithm(
              refraction_ray, inside_object_ptr ? nullptr : hit_object_casted,
              remaining_recursion - 1, max_recursion);
          pixel_value += reflection_color * fresnel_reflection_ratio;
          pixel_value += refraction_color * fresnel_transmission_ratio;
        } else {
          pixel_value += reflection_color;
        }
      }
    }

    if (inside_object_ptr) {
      std::shared_ptr<BaseObject> inside_object_casted =
          std::dynamic_pointer_cast<BaseObject>(inside_object_ptr);

      Vec3f absorption_coefficient =
          dynamic_cast<DielectricMaterial*>(
              (inside_object_casted->material_).get())
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