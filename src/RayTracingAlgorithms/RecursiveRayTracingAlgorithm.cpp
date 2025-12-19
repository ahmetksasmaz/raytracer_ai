#include <limits>

#include "Scene.hpp"

Vec3f Scene::RecursiveRayTracingAlgorithm(
    Ray &ray,
    const std::shared_ptr<BoundingVolumeHierarchyElement> inside_object_ptr,
    int remaining_recursion, int max_recursion)
{
  remaining_recursion--;
  Vec3f pixel_value = {0, 0, 0};
  FP_PRECISION t_hit = std::numeric_limits<FP_PRECISION>::max();
  Vec2f hit_tex_coords;
  Vec3f hit_normal;
  Vec3f hit_tangent_vector;
  Vec3f hit_bitangent_vector;
  Vec2f hit_u_vector;
  Vec2f hit_v_vector;
  std::shared_ptr<BoundingVolumeHierarchyElement> hit_object_ptr = nullptr;

  if (inside_object_ptr == nullptr)
  {
    if (configuration_.acceleration_.bvh_high_level_)
    {
      hit_object_ptr = bvh_root_->Intersect(ray, t_hit, hit_normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector);
    }
    else
    {
      for (auto object : objects_)
      {
        FP_PRECISION temp_hit = std::numeric_limits<FP_PRECISION>::max();
        Vec3f normal;
        std::shared_ptr<BaseObject> hit_object_casted =
            std::dynamic_pointer_cast<BaseObject>(object);
        if (object->Intersect(ray, temp_hit, normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector))
        {
          if (t_hit > temp_hit)
          {
            t_hit = temp_hit;
            hit_object_ptr = object;
            hit_normal = normal;
          }
        }
      }
    }

    for (const auto &plane : plane_objects_)
    {
      // Plane cast plane object
      auto plane_casted = std::dynamic_pointer_cast<PlaneObject>(plane);

      FP_PRECISION temp_hit = std::numeric_limits<FP_PRECISION>::max();
      Vec3f normal;
      if (plane_casted->IntersectPlane(ray, temp_hit, normal))
      {
        if (t_hit > temp_hit)
        {
          t_hit = temp_hit;
          hit_object_ptr = plane;
          hit_normal = normal;
        }
      }
    }

  }
  else
  {
    hit_object_ptr = inside_object_ptr;
    inside_object_ptr->Intersect(ray, t_hit, hit_normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, false);
    if (dot(ray.direction_, hit_normal) > 0)
    {
      hit_normal = -hit_normal;
    }
  }

  if (hit_object_ptr)
  {
    std::shared_ptr<BaseObject> hit_object_casted =
        std::dynamic_pointer_cast<BaseObject>(hit_object_ptr);
    // This is where the fun begins

    std::shared_ptr<BaseMaterial> material_ptr = hit_object_casted->material_;

    std::vector<std::shared_ptr<BaseTextureMap>> textures = hit_object_casted->textures_;

    FP_PRECISION texture_diffuse_coeff = 0.0f;
    FP_PRECISION texture_specular_coeff = 0.0f;
    FP_PRECISION texture_normal_coeff = 0.0f;
    FP_PRECISION texture_bump_coeff = 0.0f;
    Vec3f texture_diffuse_value = {0, 0, 0};
    Vec3f texture_specular_value = {0, 0, 0};
    Vec3f texture_normal_value = {0, 0, 0};
    Vec3f texture_replace_all_value = {0, 0, 0};
    Vec3f texture_bump_value_u = {0, 0, 0}, texture_bump_value_v = {0, 0, 0};

    Mat4x4f tbn_matrix;
    Vec3f normalized_tangent = normalize(hit_tangent_vector);
    Vec3f normalized_bitangent = normalize(hit_bitangent_vector);
    tbn_matrix.m[0][0] = normalized_tangent.x;
    tbn_matrix.m[1][0] = normalized_tangent.y;;
    tbn_matrix.m[2][0] = normalized_tangent.z;
    tbn_matrix.m[0][1] = normalized_bitangent.x;
    tbn_matrix.m[1][1] = normalized_bitangent.y;;
    tbn_matrix.m[2][1] = normalized_bitangent.z;
    tbn_matrix.m[0][2] = hit_normal.x;
    tbn_matrix.m[1][2] = hit_normal.y;
    tbn_matrix.m[2][2] = hit_normal.z;

    for (const auto &texture : textures)
    {

      // Find plane at hit point and hit normal
      // Ray plane intersection with direction_i_delta and direction_j_delta
      // Find texture value at other hit points

      Vec3f hit_point = ray.origin_ + ray.direction_ * t_hit;
      // Plane equation: N.(P - P0) = 0
      // P0 = hit_point
      // N = hit_normal
      // P = ray.origin_ + t * di_direction
      // t = N.(P0 - ray.origin_) / N.di_direction
      FP_PRECISION t_hit_di = dot(hit_normal, hit_point - ray.origin_) / dot(hit_normal, ray.direction_i_);
      FP_PRECISION t_hit_dj = dot(hit_normal, hit_point - ray.origin_) / dot(hit_normal, ray.direction_j_);
      
      // Compute texture coordinates at these new hit points
      Vec3f hit_point_di = ray.origin_ + ray.direction_i_ * t_hit_di;
      Vec3f hit_point_dj = ray.origin_ + ray.direction_j_ * t_hit_dj;
      Vec3f pijdi = hit_point_di - hit_point;
      Vec3f pijdj = hit_point_dj - hit_point;
      Vec3f pxyzdu = hit_tangent_vector;
      Vec3f pxyzdv = hit_bitangent_vector;
      Mat2x2f small_matrix;
      Vec2f pijdi_small;
      Vec2f pijdj_small;
      Vec2f pxyzdu_small;
      Vec2f pxyzdv_small;
      Vec2f result_di;
      Vec2f result_dj;
      if(hit_normal.x >= hit_normal.y && hit_normal.x >= hit_normal.z) {
        // x is the largest
        pijdi_small = Vec2f{pijdi.y, pijdi.z};
        pijdj_small = Vec2f{pijdj.y, pijdj.z};
        pxyzdu_small = Vec2f{pxyzdu.y, pxyzdu.z};
        pxyzdv_small = Vec2f{pxyzdv.y, pxyzdv.z};
      }
      else if(hit_normal.y >= hit_normal.x && hit_normal.y >= hit_normal.z) {
        // y is the largest
        pijdi_small = Vec2f{pijdi.x, pijdi.z};
        pijdj_small = Vec2f{pijdj.x, pijdj.z};
        pxyzdu_small = Vec2f{pxyzdu.x, pxyzdu.z};
        pxyzdv_small = Vec2f{pxyzdv.x, pxyzdv.z};
      }
      else {
        // z is the largest
        pijdi_small = Vec2f{pijdi.x, pijdi.y};
        pijdj_small = Vec2f{pijdj.x, pijdj.y};
        pxyzdu_small = Vec2f{pxyzdu.x, pxyzdu.y};
        pxyzdv_small = Vec2f{pxyzdv.x, pxyzdv.y};
      }
      small_matrix = Mat2x2f{{{pxyzdu_small.x, pxyzdv_small.x},
                              {pxyzdu_small.y, pxyzdv_small.y}}};
      FP_PRECISION det = small_matrix.m[0][0] * small_matrix.m[1][1] - small_matrix.m[0][1] * small_matrix.m[1][0];
      if (std::abs(det) < 1e-10) {
        continue;
      }
      Mat2x2f inv_small_matrix;
      inv_small_matrix.m[0][0] = small_matrix.m[1][1] / det;
      inv_small_matrix.m[0][1] = -small_matrix.m[0][1] / det;
      inv_small_matrix.m[1][0] = -small_matrix.m[1][0] / det;
      inv_small_matrix.m[1][1] = small_matrix.m[0][0] / det;
      result_di = Vec2f{inv_small_matrix.m[0][0] * pijdi_small.x + inv_small_matrix.m[0][1] * pijdi_small.y,
                        inv_small_matrix.m[1][0] * pijdi_small.x + inv_small_matrix.m[1][1] * pijdi_small.y};
      result_dj = Vec2f{inv_small_matrix.m[0][0] * pijdj_small.x + inv_small_matrix.m[0][1] * pijdj_small.y,
                              inv_small_matrix.m[1][0] * pijdj_small.x + inv_small_matrix.m[1][1] * pijdj_small.y};

      Vec3f texture_value = texture->GetColorAt(hit_tex_coords, hit_point, result_di, result_dj);

      if(texture->GetReplaceAllFlag()) {
        return texture_value;
      }

      FP_PRECISION current_texture_diffuse_coeff = texture->GetDiffuseCoefficient();
      if(current_texture_diffuse_coeff > 0.0f) {
        texture_diffuse_coeff = current_texture_diffuse_coeff;
        texture_diffuse_value = texture_value;
      }
      FP_PRECISION current_texture_specular_coeff = texture->GetSpecularCoefficient();
      if(current_texture_specular_coeff > 0.0f) {
        texture_specular_coeff = current_texture_specular_coeff;
        texture_specular_value = texture_value;
      }
      FP_PRECISION current_texture_normal_coeff = texture->GetNormalCoefficient();
      if(current_texture_normal_coeff > 0.0f) {
        texture_normal_coeff = current_texture_normal_coeff;
        texture_normal_value = normalize(texture_value * 2.0 - Vec3f{1.0f, 1.0f, 1.0f});
      }
      FP_PRECISION current_texture_bump_coeff = texture->GetBumpCoefficient();
      if(current_texture_bump_coeff > 0.0f) {
        texture_bump_coeff = current_texture_bump_coeff;
        texture->GetGradientAt(hit_tex_coords, ray.origin_ + ray.direction_ * t_hit,  hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, texture_bump_value_u, texture_bump_value_v);
      }
    }

    if(texture_normal_coeff > 0.0) {
      Vec3f modified_normal = tbn_matrix ^ texture_normal_value;
      hit_normal = normalize(modified_normal);
    }
    else if (texture_bump_coeff > 0.0)
    {
      FP_PRECISION grad_u_scalar = (texture_bump_value_u.x + texture_bump_value_u.y + texture_bump_value_u.z) / 3.0;
      FP_PRECISION grad_v_scalar = (texture_bump_value_v.x + texture_bump_value_v.y + texture_bump_value_v.z) / 3.0;

      hit_normal = normalize(hit_normal -
                             texture_bump_coeff * grad_u_scalar * normalize(hit_tangent_vector) -
                             texture_bump_coeff * grad_v_scalar * normalize(hit_bitangent_vector));
    }
    

    if (!inside_object_ptr)
    {
      if (configuration_.shading_.ambient_)
      {
        for (auto ambient_light : ambient_lights_)
        {
          
          Vec3f ambient_value = hadamard(material_ptr->ambient_, ambient_light->intensity_);
          pixel_value += ambient_value;

        }
      }
    }

    Vec3f intersection_point =
        ray.origin_ + ray.direction_ * t_hit + hit_normal * shadow_ray_epsilon_;
    if (!inside_object_ptr)
    {
      for (auto point_light : point_lights_)
      {
        Ray shadow_ray = {
            ray.pixel_, intersection_point,
            normalize(point_light->position_ - intersection_point), ray.diff_,
            ray.time_};
        FP_PRECISION distance_to_light =
            norm2(point_light->position_ - intersection_point);
        bool is_in_shadow = false;
        FP_PRECISION shadow_hit = std::numeric_limits<FP_PRECISION>::max();
        Vec2f shadow_tex_coords;
        Vec3f shadow_normal;
        Vec3f shadow_tangent_vector;
        Vec3f shadow_bitangent_vector;
        Vec2f shadow_u_vector;
        Vec2f shadow_v_vector;
        if (configuration_.acceleration_.bvh_high_level_)
        {
          auto ret = bvh_root_->Intersect(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                          false);
          if (ret && (shadow_hit < sqrt(distance_to_light)))
          {
            is_in_shadow = true;
          }
        }
        else
        {
          for (auto object : objects_)
          {
            if (object->Intersect(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                  false))
            {
              if (shadow_hit < sqrt(distance_to_light))
              {
                is_in_shadow = true;
                break;
              }
            }
          }
          for (const auto &plane : plane_objects_)
          {
            // Plane cast plane object
            auto plane_casted = std::dynamic_pointer_cast<PlaneObject>(plane);
            if (plane_casted->IntersectPlane(shadow_ray, shadow_hit, shadow_normal))
            {
              if (shadow_hit < sqrt(distance_to_light))
              {
                is_in_shadow = true;
                break;
              }
            }
          }
        }
        if (!is_in_shadow)
        {
          Vec3f light_direction =
              normalize(point_light->position_ - intersection_point);

          if (configuration_.shading_.diffuse_)
          {
            Vec3f diffuse_term =
                hadamard(material_ptr->diffuse_,
                         point_light->intensity_ / distance_to_light) *
                std::max(0.0, dot(hit_normal, light_direction));
            Vec3f texture_diffuse = hadamard(texture_diffuse_value, point_light->intensity_ / distance_to_light) *
                std::max(0.0, dot(hit_normal, light_direction));
            pixel_value += (1-texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;
          }

          if (configuration_.shading_.specular_)
          {
            if (material_ptr->phong_exponent_ >= 0.0f)
            {
              Vec3f half_vector = normalize(light_direction - ray.direction_);
              Vec3f specular_term = {0, 0, 0};
              specular_term =
                  hadamard(material_ptr->specular_,
                           point_light->intensity_ / distance_to_light) *
                  pow(std::max(0.0, dot(hit_normal, half_vector)),
                      material_ptr->phong_exponent_);
              Vec3f texture_specular = hadamard(texture_specular_value, point_light->intensity_ / distance_to_light) *
                  pow(std::max(0.0, dot(hit_normal, half_vector)),
                      material_ptr->phong_exponent_);
              pixel_value += (1-texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
            }
          }
        }
      }

      for (auto area_light : area_lights_)
      {
        std::vector<Vec2f> diff = area_light_sampling_algorithm_(1);

        Vec3f area_light_position = area_light->position_;

        Vec3f area_light_normal = -normalize(area_light->normal_);
        Vec3f normal_prime = area_light_normal;
        int min_index = 0;
        FP_PRECISION min_value = area_light_normal.x;
        if (area_light_normal.y < min_value)
        {
          min_value = area_light_normal.y;
          min_index = 1;
        }
        if (area_light_normal.z < min_value)
        {
          min_value = area_light_normal.z;
          min_index = 2;
        }
        switch (min_index)
        {
        case 0:
          normal_prime.x = 1.0f;
          break;
        case 1:
          normal_prime.y = 1.0f;
          break;
        case 2:
          normal_prime.z = 1.0f;
          break;
        }

        Vec3f u = normalize(cross(normal_prime, area_light_normal));
        Vec3f v = cross(area_light_normal, u);

        area_light_position = area_light_position + area_light->size_ * (u * (2.0 * diff[0].x - 1.0f) + v * (2.0 * diff[0].y - 1.0f));

        Ray shadow_ray = {
            ray.pixel_, intersection_point,
            normalize(area_light_position - intersection_point), ray.diff_,
            ray.time_};
        FP_PRECISION distance_to_light =
            norm2(area_light_position - intersection_point);
        bool is_in_shadow = false;
        FP_PRECISION shadow_hit = std::numeric_limits<FP_PRECISION>::max();
        Vec3f shadow_normal;
        Vec2f shadow_tex_coords;
        Vec3f shadow_tangent_vector;
        Vec3f shadow_bitangent_vector;
        Vec2f shadow_u_vector;
        Vec2f shadow_v_vector;
        if (configuration_.acceleration_.bvh_high_level_)
        {
          auto ret = bvh_root_->Intersect(shadow_ray, shadow_hit, shadow_normal,
                                          shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector, false);
          if (ret && (shadow_hit < sqrt(distance_to_light)))
          {
            is_in_shadow = true;
          }
        }
        else
        {
          for (auto object : objects_)
          {
            if (object->Intersect(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                  false))
            {
              if (shadow_hit < sqrt(distance_to_light))
              {
                is_in_shadow = true;
                break;
              }
            }
          }
          for (const auto &plane : plane_objects_)
          {
            // Plane cast plane object
            auto plane_casted = std::dynamic_pointer_cast<PlaneObject>(plane);
            if (plane_casted->IntersectPlane(shadow_ray, shadow_hit, shadow_normal))
            {
              if (shadow_hit < sqrt(distance_to_light))
              {
                is_in_shadow = true;
                break;
              }
            }
          }
        }
        if (!is_in_shadow)
        {
          Vec3f light_direction =
              normalize(area_light_position - intersection_point);

          FP_PRECISION irradiance_coeff = area_light->size_ * area_light->size_ * dot(area_light_normal, light_direction) / distance_to_light;

          irradiance_coeff = abs(irradiance_coeff);

          if (configuration_.shading_.diffuse_)
          {
            Vec3f diffuse_term =
                hadamard(material_ptr->diffuse_,
                         area_light->radiance_ * irradiance_coeff) *
                std::max(0.0, dot(hit_normal, light_direction));
            Vec3f texture_diffuse = hadamard(texture_diffuse_value, area_light->radiance_ * irradiance_coeff) *
                std::max(0.0, dot(hit_normal, light_direction));
            pixel_value += (1 - texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;
          }

          if (configuration_.shading_.specular_)
          {
            if (material_ptr->phong_exponent_ >= 0.0f)
            {
              Vec3f half_vector = normalize(light_direction - ray.direction_);
              Vec3f specular_term = {0, 0, 0};
              specular_term =
                  hadamard(material_ptr->specular_,
                           area_light->radiance_ * irradiance_coeff) *
                  pow(std::max(0.0, dot(hit_normal, half_vector)),
                      material_ptr->phong_exponent_);
              Vec3f texture_specular = hadamard(texture_specular_value, area_light->radiance_ * irradiance_coeff) *
                  pow(std::max(0.0, dot(hit_normal, half_vector)),
                      material_ptr->phong_exponent_);
              pixel_value += (1 - texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
            }
          }
        }
      }
    }

    if (remaining_recursion > 0)
    {
      MirrorMaterial *mirror_material_ptr =
          dynamic_cast<MirrorMaterial *>(material_ptr.get());
      ConductorMaterial *conductor_material_ptr =
          dynamic_cast<ConductorMaterial *>(material_ptr.get());
      DielectricMaterial *dielectric_material_ptr =
          dynamic_cast<DielectricMaterial *>(material_ptr.get());

      Vec3f distorted_normal = hit_normal;

      if (material_ptr->roughness_ > 0.0f)
      {
        Vec3f normal_prime = hit_normal;
        int min_index = 0;
        FP_PRECISION min_value = hit_normal.x;
        if (hit_normal.y < min_value)
        {
          min_value = hit_normal.y;
          min_index = 1;
        }
        if (hit_normal.z < min_value)
        {
          min_value = hit_normal.z;
          min_index = 2;
        }
        switch (min_index)
        {
        case 0:
          normal_prime.x = 1.0f;
          break;
        case 1:
          normal_prime.y = 1.0f;
          break;
        case 2:
          normal_prime.z = 1.0f;
          break;
        }

        Vec3f u = normalize(cross(normal_prime, hit_normal));
        Vec3f v = cross(hit_normal, u);

        distorted_normal = normalize(
            hit_normal + material_ptr->roughness_ *
                             (u * (((FP_PRECISION)rand() / RAND_MAX) - 0.5f) +
                              v * (((FP_PRECISION)rand() / RAND_MAX) - 0.5f)));
      }

      if (mirror_material_ptr && configuration_.materials_.mirror_)
      {
        Vec3f reflection_direction =
            ray.direction_ -
            2 * dot(ray.direction_, distorted_normal) * distorted_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction, ray.diff_, ray.time_};
        Vec3f reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion,
            max_recursion);
        pixel_value += hadamard(reflection_color, mirror_material_ptr->mirror_);
      }
      else if (conductor_material_ptr &&
               configuration_.materials_.conductor_)
      {
        Vec3f reflection_direction =
            ray.direction_ -
            2 * dot(ray.direction_, distorted_normal) * distorted_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction, ray.diff_, ray.time_};
        Vec3f reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion,
            max_recursion);

        FP_PRECISION n2 = conductor_material_ptr->refraction_index_;
        FP_PRECISION k2 = conductor_material_ptr->absorption_index_;
        FP_PRECISION cos_theta = -dot(ray.direction_, distorted_normal);
        FP_PRECISION n2_k2_2 = n2 * n2 + k2 * k2;
        FP_PRECISION n2_cos_theta_tw = 2 * n2 * cos_theta;
        FP_PRECISION cos_theta_2 = cos_theta * cos_theta;
        FP_PRECISION rs = (n2_k2_2 - n2_cos_theta_tw + cos_theta_2) /
                   (n2_k2_2 + n2_cos_theta_tw + cos_theta_2);
        FP_PRECISION rp = (n2_k2_2 * cos_theta_2 - n2_cos_theta_tw + 1) /
                   (n2_k2_2 * cos_theta_2 + n2_cos_theta_tw + 1);
        FP_PRECISION fresnel_reflection_ratio = (rs + rp) / 2;

        pixel_value +=
            hadamard(reflection_color, conductor_material_ptr->mirror_ *
                                           fresnel_reflection_ratio);
      }
      else if (dielectric_material_ptr &&
               configuration_.materials_.dielectric_)
      {
        Vec3f reflection_color = {0, 0, 0};
        Vec3f reflection_direction =
            ray.direction_ -
            2 * dot(ray.direction_, distorted_normal) * distorted_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction, ray.diff_, ray.time_};
        reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion,
            max_recursion);

        FP_PRECISION n1 = inside_object_ptr
                       ? dielectric_material_ptr->refraction_index_
                       : 1.0f;
        FP_PRECISION n2 = inside_object_ptr
                       ? 1.0
                       : dielectric_material_ptr->refraction_index_;

        FP_PRECISION cos_theta = dot(-ray.direction_, distorted_normal);
        FP_PRECISION cos_phi_2 =
            1 - (n1 * n1 / (n2 * n2)) * (1 - cos_theta * cos_theta);
        if (cos_phi_2 > 0.0)
        {
          FP_PRECISION cos_phi = sqrt(cos_phi_2);
          FP_PRECISION r_p =
              (n1 * cos_theta - n2 * cos_phi) / (n1 * cos_theta + n2 * cos_phi);
          FP_PRECISION r_s =
              (n1 * cos_phi - n2 * cos_theta) / (n1 * cos_phi + n2 * cos_theta);

          FP_PRECISION fresnel_reflection_ratio = (r_p * r_p + r_s * r_s) / 2;
          FP_PRECISION fresnel_transmission_ratio = 1.0 - fresnel_reflection_ratio;

          Vec3f refraction_direction =
              normalize((n1 / n2) * ray.direction_ +
                        (n1 / n2 * cos_theta - cos_phi) * distorted_normal);
          Ray refraction_ray = {
              ray.pixel_,
              intersection_point - 2 * shadow_ray_epsilon_ * distorted_normal,
              refraction_direction, ray.diff_, ray.time_};
          // If the object type is triangle, inside_object_ptr is nullptr, check
          // later
          Vec3f refraction_color = RecursiveRayTracingAlgorithm(
              refraction_ray, inside_object_ptr ? nullptr : hit_object_casted,
              remaining_recursion, max_recursion);
          pixel_value += reflection_color * fresnel_reflection_ratio;
          pixel_value += refraction_color * fresnel_transmission_ratio;
        }
        else
        {
          pixel_value += reflection_color;
        }
      }
    }

    if (inside_object_ptr)
    {
      std::shared_ptr<BaseObject> inside_object_casted =
          std::dynamic_pointer_cast<BaseObject>(inside_object_ptr);

      Vec3f absorption_coefficient =
          dynamic_cast<DielectricMaterial *>(
              (inside_object_casted->material_).get())
              ->absorption_coefficient_;
      pixel_value.x *= exp(-absorption_coefficient.x * t_hit);
      pixel_value.y *= exp(-absorption_coefficient.y * t_hit);
      pixel_value.z *= exp(-absorption_coefficient.z * t_hit);
    }
  }
  else
  {
    if (remaining_recursion == max_recursion-1)
    {
      pixel_value.x = background_color_.x;
      pixel_value.y = background_color_.y;
      pixel_value.z = background_color_.z;
    }
    else
    {
      pixel_value = {0, 0, 0};
    }
  }

  return pixel_value;
};