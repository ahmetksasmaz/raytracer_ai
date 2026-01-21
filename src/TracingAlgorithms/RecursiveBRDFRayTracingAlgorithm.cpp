#include <limits>

#include "Scene.hpp"

Vec3f Scene::RecursiveBRDFRayTracingAlgorithm(
    Ray &ray,
    const std::shared_ptr<BoundingVolumeHierarchyElement> inside_object_ptr,
    int remaining_recursion, int max_recursion)
{
    remaining_recursion--;
    Vec3f total_light_value = {0, 0, 0};

    // STEP : CHECK FOR INTERSECTION

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
            hit_object_ptr = bvh_root_->Intersect(ray, t_hit, hit_normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, false);
        }
        else
        {
            for (auto object : objects_)
            {
                FP_PRECISION temp_hit = std::numeric_limits<FP_PRECISION>::max();
                Vec3f normal;
                std::shared_ptr<BaseObject> hit_object_casted =
                    std::dynamic_pointer_cast<BaseObject>(object);
                if (object->Intersect(ray, temp_hit, normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, false))
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

    // STEP : IF NO INTERSECTION, RETURN BACKGROUND COLOR / ENVIRONMENT LIGHT
    if(!hit_object_ptr){
        if (remaining_recursion == max_recursion-1){
            if(spherical_directional_light_){
                Vec3f direction;
                Vec3f env_radiance = spherical_directional_light_->GetIntensity(ray.direction_, direction, true);
                total_light_value = env_radiance;
            }
            else if(background_texture_map_){
                FP_PRECISION u = 0.5 + (atan2(ray.direction_.z, ray.direction_.x) / (2 * M_PI));
                FP_PRECISION v = 0.5 - (asin(ray.direction_.y) / M_PI);
                Vec2f tex_coords = {u, v};
                total_light_value = background_texture_map_->GetColorAt(tex_coords, {0,0,0});
            }
            else{
                total_light_value.x = background_color_.x;
                total_light_value.y = background_color_.y;
                total_light_value.z = background_color_.z;
            }
        }
        else{
            if(spherical_directional_light_){
                Vec3f direction;
                Vec3f env_radiance = spherical_directional_light_->GetIntensity(ray.direction_, direction, true);
                total_light_value = env_radiance;
            }
            else{
                total_light_value = {0, 0, 0};
            }
        }
        return total_light_value * 1.0; // Take BRDF as 1
    }

    // STEP : FIND PROPERTIES OF OBJECT AND APPLY TEXTURES

    std::shared_ptr<BaseObject> hit_object_casted = std::dynamic_pointer_cast<BaseObject>(hit_object_ptr);
    std::shared_ptr<BaseMaterial> material_ptr = hit_object_casted->material_;
    std::vector<std::shared_ptr<BaseTextureMap>> textures = hit_object_casted->textures_;

    Vec3f KA = material_ptr->ambient_;
    Vec3f KD = material_ptr->diffuse_;
    Vec3f KS = material_ptr->specular_;

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

    Vec3f hit_point = ray.origin_ + ray.direction_ * t_hit;
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
        result_di = Vec2f{0,0};
        result_dj = Vec2f{0,0};
    }
    else{
        Mat2x2f inv_small_matrix;
        inv_small_matrix.m[0][0] = small_matrix.m[1][1] / det;
        inv_small_matrix.m[0][1] = -small_matrix.m[0][1] / det;
        inv_small_matrix.m[1][0] = -small_matrix.m[1][0] / det;
        inv_small_matrix.m[1][1] = small_matrix.m[0][0] / det;
        result_di = Vec2f{inv_small_matrix.m[0][0] * pijdi_small.x + inv_small_matrix.m[0][1] * pijdi_small.y,inv_small_matrix.m[1][0] * pijdi_small.x + inv_small_matrix.m[1][1] * pijdi_small.y};
        result_dj = Vec2f{inv_small_matrix.m[0][0] * pijdj_small.x + inv_small_matrix.m[0][1] * pijdj_small.y,inv_small_matrix.m[1][0] * pijdj_small.x + inv_small_matrix.m[1][1] * pijdj_small.y};
    }

    for (const auto &texture : textures)
    {
      Vec3f texture_value = texture->GetColorAt(hit_tex_coords, hit_point, result_di, result_dj);

      if(texture->GetReplaceAllFlag()) {
        return texture_value;
      }

      if(texture->GetDiffuseCoefficient() > 0.0f) {
        KD = KD * (1.0 - texture->GetDiffuseCoefficient()) + texture_value * texture->GetDiffuseCoefficient();
      }
      if(texture->GetSpecularCoefficient() > 0.0f) {
        KS = KS * (1.0 - texture->GetSpecularCoefficient()) + texture_value * texture->GetSpecularCoefficient();
      }
      if(texture->GetNormalCoefficient() > 0.0f) {
        Vec3f texture_normal_value = normalize(texture_value * 2.0 - Vec3f{1.0f, 1.0f, 1.0f});
        Vec3f modified_normal = tbn_matrix ^ texture_normal_value;
        hit_normal = normalize(modified_normal);
      }
      else if(texture->GetBumpCoefficient() > 0.0f){
        FP_PRECISION center_value = (texture_value.x + texture_value.y + texture_value.z) / 3.0f;
        hit_point = hit_point + hit_normal * center_value * texture->GetBumpCoefficient();
        Vec3f texture_bump_value_u;
        Vec3f texture_bump_value_v;
        texture->GetGradientAt(hit_tex_coords, ray.origin_ + ray.direction_ * t_hit,  hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, texture_bump_value_u, texture_bump_value_v);
        FP_PRECISION u_value = (texture_bump_value_u.x + texture_bump_value_u.y + texture_bump_value_u.z) / 3.0f;
        FP_PRECISION v_value = (texture_bump_value_v.x + texture_bump_value_v.y + texture_bump_value_v.z) / 3.0f;
        Vec3f dqdu = hit_tangent_vector + (u_value * texture->GetBumpCoefficient() * hit_normal);
        Vec3f dqdv = hit_bitangent_vector + (v_value * texture->GetBumpCoefficient() * hit_normal);

        Vec3f perturbed_normal = normalize(cross(dqdv, dqdu));
        hit_normal = perturbed_normal;
        // hit_normal = normalize(hit_normal + (u_value * texture->GetBumpCoefficient() * hit_tangent_vector) + (v_value * texture->GetBumpCoefficient() * hit_bitangent_vector)); 
      }
    }

    Vec3f intersection_point = hit_point + hit_normal * shadow_ray_epsilon_;

    // STEP : REFLECTION AND REFRACTION
    if(remaining_recursion > 0){
        // STEP : DISTORT NORMAL
        Vec3f distorted_normal = hit_normal;

        if(material_ptr->roughness_ > 0.0f){
            Vec3f normal_prime = hit_normal;
            int min_index = 0;
            FP_PRECISION min_value = std::abs(hit_normal.x);
            if (std::abs(hit_normal.y) < min_value)
            {
                min_value = std::abs(hit_normal.y);
                min_index = 1;
            }
            if (std::abs(hit_normal.z) < min_value)
            {
                min_value = std::abs(hit_normal.z);
                min_index = 2;
            }
            switch (min_index)
            {
                case 0:
                    normal_prime = Vec3f{0.0f, hit_normal.z, -hit_normal.y};
                    break;
                case 1:
                    normal_prime = Vec3f{hit_normal.z, 0.0f, -hit_normal.x};
                    break;
                case 2:
                    normal_prime = Vec3f{hit_normal.y, -hit_normal.x, 0.0f};
                    break;
            }

            Vec3f u = normalize(cross(normal_prime, hit_normal));
            Vec3f v = cross(hit_normal, u);

            distorted_normal = normalize(
                hit_normal + material_ptr->roughness_ *
                                (u * (((FP_PRECISION)rand() / RAND_MAX) - 0.5f) +
                                v * (((FP_PRECISION)rand() / RAND_MAX) - 0.5f)));
        }

        // STEP : HANDLE MIRROR, CONDUCTOR, DIELECTRIC MATERIALS
        MirrorMaterial *mirror_material_ptr = dynamic_cast<MirrorMaterial *>(material_ptr.get());
        ConductorMaterial *conductor_material_ptr = dynamic_cast<ConductorMaterial *>(material_ptr.get());
        DielectricMaterial *dielectric_material_ptr = dynamic_cast<DielectricMaterial *>(material_ptr.get());

        Vec3f reflection_direction = ray.direction_ - 2 * dot(ray.direction_, distorted_normal) * distorted_normal;
        Ray reflection_ray = {ray.pixel_, intersection_point, reflection_direction, ray.diff_, ray.time_};
        Vec3f reflection_color = RecursiveBRDFRayTracingAlgorithm(reflection_ray, inside_object_ptr, remaining_recursion, max_recursion);

        Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, reflection_direction, distorted_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
        if(mirror_material_ptr){
            total_light_value += hadamard(reflection_color, hadamard(mirror_material_ptr->mirror_, brdf));
        }
        else if(conductor_material_ptr){
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

            total_light_value += hadamard(reflection_color, hadamard(conductor_material_ptr->mirror_ * fresnel_reflection_ratio, brdf));
        }
        else if(dielectric_material_ptr){
            FP_PRECISION n1 = inside_object_ptr ? dielectric_material_ptr->refraction_index_ : 1.0;
            FP_PRECISION n2 = inside_object_ptr ? 1.0 : dielectric_material_ptr->refraction_index_;
            FP_PRECISION cos_theta = dot(-ray.direction_, distorted_normal);
            FP_PRECISION cos_phi_2 = 1 - (n1 * n1 / (n2 * n2)) * (1 - cos_theta * cos_theta);
            if (cos_phi_2 > 0.0){
                FP_PRECISION cos_phi = sqrt(cos_phi_2);
                FP_PRECISION r_p = (n1 * cos_theta - n2 * cos_phi) / (n1 * cos_theta + n2 * cos_phi);
                FP_PRECISION r_s = (n1 * cos_phi - n2 * cos_theta) / (n1 * cos_phi + n2 * cos_theta);
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
                Vec3f refraction_color = RecursiveBRDFRayTracingAlgorithm(
                    refraction_ray, inside_object_ptr ? nullptr : hit_object_casted,
                    remaining_recursion, max_recursion);
                total_light_value += hadamard(reflection_color, brdf) * fresnel_reflection_ratio;
                total_light_value += refraction_color * fresnel_transmission_ratio;
            }
            else
            {
                total_light_value += hadamard(reflection_color, brdf);
            }
        }
    }

    // STEP : HANDLE INSIDE HIT
    if(inside_object_ptr){
        std::shared_ptr<BaseObject> inside_object_casted =
          std::dynamic_pointer_cast<BaseObject>(inside_object_ptr);

      Vec3f absorption_coefficient =
          dynamic_cast<DielectricMaterial *>(
              (inside_object_casted->material_).get())
              ->absorption_coefficient_;
      total_light_value.x *= exp(-absorption_coefficient.x * t_hit);
      total_light_value.y *= exp(-absorption_coefficient.y * t_hit);
      total_light_value.z *= exp(-absorption_coefficient.z * t_hit);

      return total_light_value * 1.0; // Take BRDF as 1
    }

    // STEP : HANDLE OUTSIDE HIT
    std::function<bool(Vec3f, FP_PRECISION)> ShadowCheck = [&](Vec3f direction, FP_PRECISION max_distance) {
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
        if (configuration_.acceleration_.bvh_high_level_)
        {
          auto ret = bvh_root_->Intersect(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,false);
          if (ret && shadow_hit < max_distance){is_in_shadow = true;}
        }
        else
        {
          for (auto object : objects_)
          {
            if (object->Intersect(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords, shadow_u_vector, shadow_v_vector, shadow_tangent_vector, shadow_bitangent_vector,false) && shadow_hit < max_distance)
            {
              is_in_shadow = true;
              break;
            }
          }
          for (const auto &plane : plane_objects_)
          {
            // Plane cast plane object
            auto plane_casted = std::dynamic_pointer_cast<PlaneObject>(plane);
            if (plane_casted->IntersectPlane(shadow_ray, shadow_hit, shadow_normal) && shadow_hit < max_distance)
            {
              is_in_shadow = true;
              break;
            }
          }
        }
        return is_in_shadow;
    };

    // STEP : LIGHT OBJECT HITS
    auto light_object_casted = std::dynamic_pointer_cast<ObjectLightSource>(hit_object_ptr);
    if (light_object_casted){
        return light_object_casted->radiance_;
    }

    // STEP : ACCUMULATE LIGHT CONTRIBUTIONS
    if(spherical_directional_light_){
        Vec3f direction;
        Vec3f env_radiance = spherical_directional_light_->GetIntensity(hit_normal, direction);
        bool is_in_shadow = ShadowCheck(direction, std::numeric_limits<FP_PRECISION>::max());
        if(!is_in_shadow){
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, direction, hit_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(env_radiance, brdf) * std::max(0.0, dot(hit_normal, direction));
        }
    }
    for(auto& point_light : point_lights_)
    {
        Vec3f light_direction = normalize(point_light->position_ - intersection_point);
        FP_PRECISION distance_to_light = norm(point_light->position_ - intersection_point);
        bool is_in_shadow = ShadowCheck(light_direction, distance_to_light);
        if(!is_in_shadow){
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_direction, hit_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(point_light->intensity_, brdf) * std::max(0.0, dot(hit_normal, light_direction)) / (distance_to_light * distance_to_light);
        }
    }
    for(auto& area_light: area_lights_)
    {
        std::vector<Vec2f> diff = area_light_sampling_algorithm_(1);
        Vec3f area_light_position = area_light->position_;
        Vec3f area_light_normal = normalize(area_light->normal_);
        Vec3f normal_prime = area_light_normal;
        int min_index = 0;
        FP_PRECISION min_value = std::abs(area_light_normal.x);
        if (std::abs(area_light_normal.y) < min_value)
        {
          min_value = std::abs(area_light_normal.y);
          min_index = 1;
        }
        if (std::abs(area_light_normal.z) < min_value)
        {
          min_value = std::abs(area_light_normal.z);
          min_index = 2;
        }
        switch (min_index)
        {
            case 0:
            normal_prime = Vec3f{0.0f, area_light_normal.z, -area_light_normal.y};
            break;
            case 1:
            normal_prime = Vec3f{area_light_normal.z, 0.0f, -area_light_normal.x};
            break;
            case 2:
            normal_prime = Vec3f{area_light_normal.y, -area_light_normal.x, 0.0f};
            break;
        }
        Vec3f u = normalize(cross(normal_prime, area_light_normal));
        Vec3f v = cross(area_light_normal, u);
        area_light_position = area_light_position + area_light->size_ * (u * (diff[0].x - 0.5f) + v * (diff[0].y - 0.5f));
        Vec3f light_direction = normalize(area_light_position - intersection_point);
        FP_PRECISION distance_to_light = norm(area_light_position - intersection_point);
        bool is_in_shadow = ShadowCheck(light_direction, distance_to_light);
        if(!is_in_shadow){
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_direction, hit_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(area_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_direction)) * area_light->size_ * area_light->size_ * dot(-area_light_normal, light_direction) / (distance_to_light * distance_to_light);
        }
    }
    for(auto& spot_light : spot_lights_)
    {
        Vec3f direction_from_light = normalize(intersection_point - spot_light->position_);
        FP_PRECISION distance_to_light = norm(spot_light->position_ - intersection_point);
        FP_PRECISION angle = acos(dot(direction_from_light, normalize(spot_light->direction_)));
        FP_PRECISION coverage_radian = spot_light->coverage_angle_ * M_PI / 180.0f;
        FP_PRECISION falloff_radian = spot_light->falloff_angle_ * M_PI / 180.0f;
        if(angle <= coverage_radian / 2.0f)
        {
            Vec3f light_direction = normalize(spot_light->position_ - intersection_point);
            bool is_in_shadow = ShadowCheck(light_direction, distance_to_light);
            if(!is_in_shadow){
                Vec3f value;
                if(angle > falloff_radian / 2.0f){
                    FP_PRECISION coeff_s = (cos(angle) - cos(coverage_radian / 2.0f)) /
                                        (cos(falloff_radian / 2.0f) - cos(coverage_radian / 2.0f));
                    coeff_s = coeff_s * coeff_s; // Power of 2
                    coeff_s = coeff_s * coeff_s; // Power of 4
                    value = spot_light->intensity_ * coeff_s / (distance_to_light * distance_to_light);
                }
                else{
                    value = spot_light->intensity_ / (distance_to_light * distance_to_light);
                }
                Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_direction, hit_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
                total_light_value += hadamard(value, brdf) * std::max(0.0, dot(hit_normal, light_direction));
            }
        }
    }
    for(auto& dir_light : directional_lights_)
    {
        Vec3f light_direction = -normalize(dir_light->direction_);
        bool is_in_shadow = ShadowCheck(light_direction, std::numeric_limits<FP_PRECISION>::max());
        if(!is_in_shadow){
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_direction, hit_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(dir_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_direction));
        }
    }
    
    // STEP : OBJECT LIGHT SAMPLING
    {
        int object_light_count = light_objects_.size();
        if(object_light_count > 0){
            int random_light_index = rand() % object_light_count;
            auto light_object = light_objects_[random_light_index];
            auto light_object_casted = std::dynamic_pointer_cast<ObjectLightSource>(light_object);
            Vec3f sample_point;
            Vec3f sample_normal;
            FP_PRECISION pdf_value = 0.0;

            light_object_casted->Sample(intersection_point, sample_point, sample_normal, pdf_value);
            if(pdf_value > 0.0){
                pdf_value /= static_cast<FP_PRECISION>(object_light_count);
                Vec3f light_direction = normalize(sample_point - intersection_point);
                FP_PRECISION distance_to_light = norm(sample_point - intersection_point);
                bool is_in_shadow = ShadowCheck(light_direction, distance_to_light);
                if(!is_in_shadow){
                    Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_direction, hit_normal, KD, KS, material_ptr->refraction_index_, material_ptr->absorption_index_);
                    total_light_value += hadamard(light_object_casted->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_direction)) / pdf_value;
                }
            }
        }
    }
    
    total_light_value += hadamard(ambient_light_->intensity_, KA);

    return total_light_value;
};