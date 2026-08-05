#include <limits>

#include "Scene.hpp"

Spectrum Scene::RecursiveRayTracingAlgorithm(
    Ray &ray,
    const std::shared_ptr<BaseObject> inside_object_ptr,
    int remaining_recursion, int max_recursion)
{
  remaining_recursion--;
  Spectrum pixel_value;
  FP_PRECISION t_hit = std::numeric_limits<FP_PRECISION>::max();
  Vec2f hit_tex_coords;
  Vec3f hit_normal;
  Vec3f hit_tangent_vector;
  Vec3f hit_bitangent_vector;
  Vec2f hit_u_vector;
  Vec2f hit_v_vector;
  std::shared_ptr<BaseObject> hit_object_ptr = nullptr;

  if (inside_object_ptr == nullptr)
  {
    int hit_index = bvh_.Intersect(ray, objects_, t_hit, hit_normal, hit_tex_coords,
                                hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector);
    if (hit_index >= 0) {
      hit_object_ptr = objects_[hit_index];
    }

    for (const auto &plane : plane_objects_)
    {
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
    hit_object_ptr = std::const_pointer_cast<BaseObject>(inside_object_ptr);
    inside_object_ptr->Intersect(ray, t_hit, hit_normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, false);
    if (dot(ray.direction_, hit_normal) > 0)
    {
      hit_normal = -hit_normal;
    }
  }

  if (hit_object_ptr)
  {
    // This is where the fun begins

    std::shared_ptr<BaseMaterial> material_ptr = hit_object_ptr->material_;

    std::vector<std::shared_ptr<BaseTextureMap>> textures = hit_object_ptr->textures_;

    FP_PRECISION texture_diffuse_coeff = 0.0f;
    FP_PRECISION texture_specular_coeff = 0.0f;
    FP_PRECISION texture_normal_coeff = 0.0f;
    FP_PRECISION texture_bump_coeff = 0.0f;
    Spectrum texture_diffuse_value;
    Spectrum texture_specular_value;
    Vec3f texture_normal_value = {0, 0, 0};
    Vec3f texture_replace_all_value = {0, 0, 0};
    Vec3f texture_bump_value_center = {0,0,0}, texture_bump_value_u = {0, 0, 0}, texture_bump_value_v = {0, 0, 0};

    Mat4x4f tbn_matrix;
    Vec3f normalized_tangent = normalize(hit_tangent_vector);
    Vec3f normalized_bitangent = normalize(hit_bitangent_vector);
    tbn_matrix.m[0][0] = normalized_tangent.x;
    tbn_matrix.m[1][0] = normalized_tangent.y;
    tbn_matrix.m[2][0] = normalized_tangent.z;
    tbn_matrix.m[0][1] = normalized_bitangent.x;
    tbn_matrix.m[1][1] = normalized_bitangent.y;
    tbn_matrix.m[2][1] = normalized_bitangent.z;
    tbn_matrix.m[0][2] = hit_normal.x;
    tbn_matrix.m[1][2] = hit_normal.y;
    tbn_matrix.m[2][2] = hit_normal.z;

    for (const auto &texture : textures)
    {

      Vec3f hit_point = ray.origin_ + ray.direction_ * t_hit;
      // Plane equation: N.(P - P0) = 0
      // P0 = hit_point
      // N = hit_normal
      // P = ray.origin_ + t * di_direction
      // t = N.(P0 - ray.origin_) / N.di_direction
      FP_PRECISION t_hit_di = dot(hit_normal, hit_point - ray.origin_) / dot(hit_normal, ray.direction_i_);
      FP_PRECISION t_hit_dj = dot(hit_normal, hit_point - ray.origin_) / dot(hit_normal, ray.direction_j_);
      
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
        return UpliftRGB(texture_value);
      }

      FP_PRECISION current_texture_diffuse_coeff = texture->GetDiffuseCoefficient();
      if(current_texture_diffuse_coeff > 0.0f) {
        texture_diffuse_coeff = current_texture_diffuse_coeff;
        texture_diffuse_value = UpliftRGB(texture_value);
      }
      FP_PRECISION current_texture_specular_coeff = texture->GetSpecularCoefficient();
      if(current_texture_specular_coeff > 0.0f) {
        texture_specular_coeff = current_texture_specular_coeff;
        texture_specular_value = UpliftRGB(texture_value);
      }
      FP_PRECISION current_texture_normal_coeff = texture->GetNormalCoefficient();
      if(current_texture_normal_coeff > 0.0f) {
        texture_normal_coeff = current_texture_normal_coeff;
        texture_normal_value = normalize(texture_value * 2.0 - Vec3f{1.0f, 1.0f, 1.0f});
      }
      FP_PRECISION current_texture_bump_coeff = texture->GetBumpCoefficient();
      if(current_texture_bump_coeff > 0.0f) {
        texture_bump_coeff = current_texture_bump_coeff;
        texture_bump_value_center = texture_value;
        texture->GetGradientAt(hit_tex_coords, ray.origin_ + ray.direction_ * t_hit,  hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, texture_bump_value_u, texture_bump_value_v);
      }
    }

    Vec3f intersection_point =
        ray.origin_ + ray.direction_ * t_hit;

    if(texture_normal_coeff > 0.0) {
      Vec3f modified_normal = tbn_matrix ^ texture_normal_value;
      hit_normal = normalize(modified_normal);
    }
    else if (texture_bump_coeff > 0.0)
    {
      FP_PRECISION center_value = (texture_bump_value_center.x + texture_bump_value_center.y + texture_bump_value_center.z) / 3.0f;
      intersection_point = intersection_point + hit_normal * center_value * texture_bump_coeff;
      
      FP_PRECISION u_value = (texture_bump_value_u.x + texture_bump_value_u.y + texture_bump_value_u.z) / 3.0f;
      FP_PRECISION v_value = (texture_bump_value_v.x + texture_bump_value_v.y + texture_bump_value_v.z) / 3.0f;
      Vec3f dqdu = hit_tangent_vector + (u_value * texture_bump_coeff * hit_normal);
      Vec3f dqdv = hit_bitangent_vector + (v_value * texture_bump_coeff * hit_normal);

      Vec3f perturbed_normal = normalize(cross(dqdv, dqdu));
      hit_normal = perturbed_normal;
    }

    intersection_point = intersection_point + hit_normal * shadow_ray_epsilon_;
    

    if (!inside_object_ptr)
    {
      Spectrum ambient_value = hadamard(material_ptr->ambient_, ambient_light_->intensity_);
      pixel_value += ambient_value;

      if(spherical_directional_light_){
        Vec3f direction;
        Spectrum env_radiance = UpliftRGB(spherical_directional_light_->GetIntensity(hit_normal, direction));

        Ray shadow_ray = {
            ray.pixel_, intersection_point,
            direction, ray.diff_,
            ray.time_};
        bool is_in_shadow = false;
        FP_PRECISION shadow_hit = std::numeric_limits<FP_PRECISION>::max();
        Vec2f shadow_tex_coords;
        Vec3f shadow_normal;
        Vec3f shadow_tangent_vector;
        Vec3f shadow_bitangent_vector;
        Vec2f shadow_u_vector;
        Vec2f shadow_v_vector;
        int ret = bvh_.Intersect(shadow_ray, objects_, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                        false);
        if (ret >= 0)
        {
          is_in_shadow = true;
        }
        if (!is_in_shadow)
        {
          Spectrum diffuse_term = hadamard(material_ptr->diffuse_, env_radiance) *
            std::max(0.0, dot(hit_normal, direction));
          Spectrum texture_diffuse = hadamard(texture_diffuse_value, env_radiance) *
              std::max(0.0, dot(hit_normal, direction));
          pixel_value += (1-texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;
          if (material_ptr->phong_exponent_ >= 0.0f)
          {
            Vec3f half_vector = normalize(direction - ray.direction_);
            Spectrum specular_term;
            specular_term =
            hadamard(material_ptr->specular_,
              env_radiance) *
              pow(std::max(0.0, dot(hit_normal, half_vector)),
              material_ptr->phong_exponent_);
            Spectrum texture_specular = hadamard(texture_specular_value, env_radiance) *
              pow(std::max(0.0, dot(hit_normal, half_vector)),
              material_ptr->phong_exponent_);
            pixel_value += (1-texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
          }
        }
      }

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
        int ret = bvh_.Intersect(shadow_ray, objects_, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                        false);
        if (ret && (shadow_hit < sqrt(distance_to_light)))
        {
          is_in_shadow = true;
        }
        if (!is_in_shadow)
        {
          Vec3f light_direction =
              normalize(point_light->position_ - intersection_point);

          Spectrum diffuse_term =
              hadamard(material_ptr->diffuse_,
                       point_light->intensity_ / distance_to_light) *
              std::max(0.0, dot(hit_normal, light_direction));
          Spectrum texture_diffuse = hadamard(texture_diffuse_value, point_light->intensity_ / distance_to_light) *
              std::max(0.0, dot(hit_normal, light_direction));
          pixel_value += (1-texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;

          if (material_ptr->phong_exponent_ >= 0.0f)
          {
            Vec3f half_vector = normalize(light_direction - ray.direction_);
            Spectrum specular_term;
            specular_term =
                hadamard(material_ptr->specular_,
                         point_light->intensity_ / distance_to_light) *
                pow(std::max(0.0, dot(hit_normal, half_vector)),
                    material_ptr->phong_exponent_);
            Spectrum texture_specular = hadamard(texture_specular_value, point_light->intensity_ / distance_to_light) *
                pow(std::max(0.0, dot(hit_normal, half_vector)),
                    material_ptr->phong_exponent_);
            pixel_value += (1-texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
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
        int ret = bvh_.Intersect(shadow_ray, objects_, shadow_hit, shadow_normal,
                                        shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector, false, true);
        if (ret >= 0 && (shadow_hit < sqrt(distance_to_light)))
        {
          is_in_shadow = true;
        }
        if (!is_in_shadow)
        {
          Vec3f light_direction =
              normalize(area_light_position - intersection_point);

          FP_PRECISION irradiance_coeff = area_light->size_ * area_light->size_ * dot(area_light_normal, light_direction) / distance_to_light;

          irradiance_coeff = abs(irradiance_coeff);

          Spectrum diffuse_term =
              hadamard(material_ptr->diffuse_,
                       area_light->radiance_ * irradiance_coeff) *
              std::max(0.0, dot(hit_normal, light_direction));
          Spectrum texture_diffuse = hadamard(texture_diffuse_value, area_light->radiance_ * irradiance_coeff) *
              std::max(0.0, dot(hit_normal, light_direction));
          pixel_value += (1 - texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;

          if (material_ptr->phong_exponent_ >= 0.0f)
          {
            Vec3f half_vector = normalize(light_direction - ray.direction_);
            Spectrum specular_term;
            specular_term =
                hadamard(material_ptr->specular_,
                         area_light->radiance_ * irradiance_coeff) *
                pow(std::max(0.0, dot(hit_normal, half_vector)),
                    material_ptr->phong_exponent_);
            Spectrum texture_specular = hadamard(texture_specular_value, area_light->radiance_ * irradiance_coeff) *
                pow(std::max(0.0, dot(hit_normal, half_vector)),
                    material_ptr->phong_exponent_);
            pixel_value += (1 - texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
          }
        }
      }

      for(auto& spot_light : spot_lights_)
      {
        Vec3f direction_from_light = normalize(intersection_point - spot_light->position_);
        FP_PRECISION angle = acos(dot(direction_from_light, normalize(spot_light->direction_)));
        FP_PRECISION coverage_radian = spot_light->coverage_angle_ * M_PI / 180.0f;
        FP_PRECISION falloff_radian = spot_light->falloff_angle_ * M_PI / 180.0f;
        if(angle > coverage_radian / 2.0f)
        {
          continue;
        }
        Spectrum value;
        if(angle > falloff_radian / 2.0f){
          FP_PRECISION coeff_s = (cos(angle) - cos(coverage_radian / 2.0f)) /
                                (cos(falloff_radian / 2.0f) - cos(coverage_radian / 2.0f));
          coeff_s = coeff_s * coeff_s; // Power of 2
          coeff_s = coeff_s * coeff_s; // Power of 4
          value = spot_light->intensity_ * coeff_s / norm2(spot_light->position_ - intersection_point);
        }
        else{
          value = spot_light->intensity_ / norm2(spot_light->position_ - intersection_point);
        }

        Ray shadow_ray = {
            ray.pixel_, intersection_point,
            -direction_from_light, ray.diff_,
            ray.time_};
        bool is_in_shadow = false;
        FP_PRECISION shadow_hit = std::numeric_limits<FP_PRECISION>::max();
        Vec2f shadow_tex_coords;
        Vec3f shadow_normal;
        Vec3f shadow_tangent_vector;
        Vec3f shadow_bitangent_vector;
        Vec2f shadow_u_vector;
        Vec2f shadow_v_vector;
        int ret = bvh_.Intersect(shadow_ray, objects_, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                        false);
        if (ret >= 0)
        {
          is_in_shadow = true;
        }
        if (!is_in_shadow)
        {
          Spectrum diffuse_term = hadamard(material_ptr->diffuse_, value) *
            std::max(0.0, dot(hit_normal, -direction_from_light));
          Spectrum texture_diffuse = hadamard(texture_diffuse_value, value) *
              std::max(0.0, dot(hit_normal, -direction_from_light));
          pixel_value += (1-texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;
          if (material_ptr->phong_exponent_ >= 0.0f)
          {
            Vec3f half_vector = normalize(-direction_from_light - ray.direction_);
            Spectrum specular_term;
            specular_term =
            hadamard(material_ptr->specular_,
              value) *
              pow(std::max(0.0, dot(hit_normal, half_vector)),
              material_ptr->phong_exponent_);
            Spectrum texture_specular = hadamard(texture_specular_value, value) *
              pow(std::max(0.0, dot(hit_normal, half_vector)),
              material_ptr->phong_exponent_);
            pixel_value += (1-texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
          }
        }
      }
      for(auto& directional_light : directional_lights_)
      {
        Vec3f light_direction = -normalize(directional_light->direction_);

        Ray shadow_ray = {
            ray.pixel_, intersection_point,
            light_direction, ray.diff_,
            ray.time_};
        bool is_in_shadow = false;
        FP_PRECISION shadow_hit = std::numeric_limits<FP_PRECISION>::max();
        Vec2f shadow_tex_coords;
        Vec3f shadow_normal;
        Vec3f shadow_tangent_vector;
        Vec3f shadow_bitangent_vector;
        Vec2f shadow_u_vector;
        Vec2f shadow_v_vector;
        int ret = bvh_.Intersect(shadow_ray, objects_, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,
                                        false);
        if (ret >= 0)
        {
          is_in_shadow = true;
        }
        if (!is_in_shadow)
        {
          Spectrum diffuse_term = hadamard(material_ptr->diffuse_,
                       directional_light->radiance_) *
              std::max(0.0, dot(hit_normal, light_direction));
          Spectrum texture_diffuse = hadamard(texture_diffuse_value,
                       directional_light->radiance_) *
              std::max(0.0, dot(hit_normal, light_direction));
          pixel_value += (1-texture_diffuse_coeff) * diffuse_term + texture_diffuse_coeff * texture_diffuse;
          if (material_ptr->phong_exponent_ >= 0.0f)
          {
            Vec3f half_vector = normalize(light_direction - ray.direction_);
            Spectrum specular_term;
            specular_term =
            hadamard(material_ptr->specular_,
              directional_light->radiance_) *
              pow(std::max(0.0, dot(hit_normal, half_vector)),
              material_ptr->phong_exponent_);
            Spectrum texture_specular = hadamard(texture_specular_value,
              directional_light->radiance_) *
              pow(std::max(0.0, dot(hit_normal, half_vector)),
              material_ptr->phong_exponent_);
            pixel_value += (1-texture_specular_coeff) * specular_term + texture_specular_coeff * texture_specular;
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
                             (u * (FastRandom() - 0.5f) +
                              v * (FastRandom() - 0.5f)));
      }

      if (mirror_material_ptr)
      {
        Vec3f reflection_direction =
            ray.direction_ -
            2 * dot(ray.direction_, distorted_normal) * distorted_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction, ray.diff_, ray.time_};
        Spectrum reflection_color = RecursiveRayTracingAlgorithm(
            reflection_ray, inside_object_ptr, remaining_recursion,
            max_recursion);
        pixel_value += hadamard(reflection_color, mirror_material_ptr->mirror_);
      }
      else if (conductor_material_ptr)
      {
        Vec3f reflection_direction =
            ray.direction_ -
            2 * dot(ray.direction_, distorted_normal) * distorted_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point,
                              reflection_direction, ray.diff_, ray.time_};
        Spectrum reflection_color = RecursiveRayTracingAlgorithm(
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
      else if (dielectric_material_ptr)
      {
        Spectrum reflection_color;
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
          FP_PRECISION r_s =
              (n1 * cos_theta - n2 * cos_phi) / (n1 * cos_theta + n2 * cos_phi);
          FP_PRECISION r_p =
              (n2 * cos_theta - n1 * cos_phi) / (n2 * cos_theta + n1 * cos_phi);

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
          Spectrum refraction_color = RecursiveRayTracingAlgorithm(
              refraction_ray, inside_object_ptr ? nullptr : hit_object_ptr,
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

      Spectrum absorption_coefficient =
          dynamic_cast<DielectricMaterial *>(
              (inside_object_casted->material_).get())
              ->absorption_coefficient_;
      for (int band = 0; band < kSpectralBands; band++) {
        pixel_value[band] *= std::exp(-absorption_coefficient[band] * t_hit);
      }
    }
  }
  else
  {
    if (remaining_recursion == max_recursion-1)
    {
      if(spherical_directional_light_){
        Vec3f direction;
        Spectrum env_radiance = UpliftRGB(spherical_directional_light_->GetIntensity(ray.direction_, direction, true));
        pixel_value = env_radiance;
      }
      else if(background_texture_map_)
      {
        FP_PRECISION u = 0.5 + (atan2(ray.direction_.z, ray.direction_.x) / (2 * M_PI));
        FP_PRECISION v = 0.5 - (asin(ray.direction_.y) / M_PI);
        Vec2f tex_coords = {u, v};
        pixel_value = UpliftRGB(background_texture_map_->GetColorAt(tex_coords, {0,0,0}));
      }
      else
      {
        pixel_value = background_color_;
      }
    }
    else
    {
      if(spherical_directional_light_){
        Vec3f direction;
        Spectrum env_radiance = UpliftRGB(spherical_directional_light_->GetIntensity(ray.direction_, direction, true));
        pixel_value = env_radiance;
      }
      else{
        pixel_value = Spectrum();
      }
    }
  }

  return pixel_value;
};