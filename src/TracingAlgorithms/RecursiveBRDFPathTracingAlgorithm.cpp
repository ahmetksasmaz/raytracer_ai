#include <cmath>
#include <limits>

#include "Scene.hpp"

Vec3f Scene::RecursiveBRDFPathTracingAlgorithm(
    Ray &ray,
    const std::shared_ptr<BaseObject> inside_object_ptr,
    int current_recursion, const PathTracerSettings& settings, Vec3f throughput)
{
    current_recursion++;
    Vec3f total_light_value = {0, 0, 0};
    
    const int sample_count = (current_recursion == 1) ? settings.splitting_factor : 1;
    
    const int HARD_MAX_DEPTH = 32;
    if (current_recursion > HARD_MAX_DEPTH) {
        return {0, 0, 0};
    }
    
    if (settings.russian_roulette_enabled && current_recursion > settings.min_recursion_depth) {
        const FP_PRECISION max_throughput = std::max({throughput.x, throughput.y, throughput.z});
        const FP_PRECISION rr_probability = std::min(max_throughput, static_cast<FP_PRECISION>(0.95));
        if (FastRandom() > rr_probability) {
            return {0, 0, 0};
        }
    }

    // STEP : CHECK FOR INTERSECTION
    FP_PRECISION t_hit = std::numeric_limits<FP_PRECISION>::max();
    Vec2f hit_tex_coords;
    Vec3f hit_normal;
    Vec3f hit_tangent_vector;
    Vec3f hit_bitangent_vector;
    Vec2f hit_u_vector;
    Vec2f hit_v_vector;
    std::shared_ptr<BaseObject> hit_object_ptr = nullptr;

    if (inside_object_ptr == nullptr) {
        int hit_index = bvh_.Intersect(ray, objects_, t_hit, hit_normal, hit_tex_coords,
                                    hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, false);
        if (hit_index >= 0) {
            hit_object_ptr = objects_[hit_index];
        }

        for (const auto &plane : plane_objects_) {
            auto plane_casted = std::dynamic_pointer_cast<PlaneObject>(plane);
            FP_PRECISION temp_hit = std::numeric_limits<FP_PRECISION>::max();
            Vec3f normal;
            if (plane_casted->IntersectPlane(ray, temp_hit, normal) && t_hit > temp_hit) {
                t_hit = temp_hit;
                hit_object_ptr = plane;
                hit_normal = normal;
            }
        }
    } else {
        hit_object_ptr = std::const_pointer_cast<BaseObject>(inside_object_ptr);
        inside_object_ptr->Intersect(ray, t_hit, hit_normal, hit_tex_coords, hit_u_vector, hit_v_vector, hit_tangent_vector, hit_bitangent_vector, false);
        if (dot(ray.direction_, hit_normal) > 0) {
            hit_normal = -hit_normal;
        }
    }

    // NO INTERSECTION
    if (!hit_object_ptr) {
        if (spherical_directional_light_) {
            Vec3f direction;
            return spherical_directional_light_->GetIntensity(ray.direction_, direction, true);
        }
        if (current_recursion == 1) {
            if (background_texture_map_) {
                FP_PRECISION u = 0.5 + (atan2(ray.direction_.z, ray.direction_.x) / (2 * M_PI));
                FP_PRECISION v = 0.5 - (asin(ray.direction_.y) / M_PI);
                return background_texture_map_->GetColorAt({u, v}, {0,0,0});
            }
            return {static_cast<FP_PRECISION>(background_color_.x), 
                    static_cast<FP_PRECISION>(background_color_.y), 
                    static_cast<FP_PRECISION>(background_color_.z)};
        }
        return {0, 0, 0};
    }

    // STEP : FIND PROPERTIES OF OBJECT AND APPLY TEXTURES
    std::shared_ptr<BaseMaterial> material_ptr = hit_object_ptr->material_;
    const std::vector<std::shared_ptr<BaseTextureMap>>& textures = hit_object_ptr->textures_;

    Vec3f KD = material_ptr->diffuse_;
    Vec3f KS = material_ptr->specular_;

    // TBN matrix for normal mapping
    Vec3f normalized_tangent = normalize(hit_tangent_vector);
    Vec3f normalized_bitangent = normalize(hit_bitangent_vector);
    Mat4x4f tbn_matrix;
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
    Vec3f pijdi = (ray.origin_ + ray.direction_i_ * t_hit_di) - hit_point;
    Vec3f pijdj = (ray.origin_ + ray.direction_j_ * t_hit_dj) - hit_point;
    
    Vec2f result_di = {0, 0}, result_dj = {0, 0};
    Vec2f pijdi_small, pijdj_small, pxyzdu_small, pxyzdv_small;
    
    if (hit_normal.x >= hit_normal.y && hit_normal.x >= hit_normal.z) {
        pijdi_small = {pijdi.y, pijdi.z};
        pijdj_small = {pijdj.y, pijdj.z};
        pxyzdu_small = {hit_tangent_vector.y, hit_tangent_vector.z};
        pxyzdv_small = {hit_bitangent_vector.y, hit_bitangent_vector.z};
    } else if (hit_normal.y >= hit_normal.x && hit_normal.y >= hit_normal.z) {
        pijdi_small = {pijdi.x, pijdi.z};
        pijdj_small = {pijdj.x, pijdj.z};
        pxyzdu_small = {hit_tangent_vector.x, hit_tangent_vector.z};
        pxyzdv_small = {hit_bitangent_vector.x, hit_bitangent_vector.z};
    } else {
        pijdi_small = {pijdi.x, pijdi.y};
        pijdj_small = {pijdj.x, pijdj.y};
        pxyzdu_small = {hit_tangent_vector.x, hit_tangent_vector.y};
        pxyzdv_small = {hit_bitangent_vector.x, hit_bitangent_vector.y};
    }
    
    FP_PRECISION det = pxyzdu_small.x * pxyzdv_small.y - pxyzdu_small.y * pxyzdv_small.x;
    if (std::abs(det) >= 1e-10) {
        FP_PRECISION inv_det = 1.0 / det;
        Mat2x2f inv = {{{pxyzdv_small.y * inv_det, -pxyzdu_small.y * inv_det},
                        {-pxyzdv_small.x * inv_det, pxyzdu_small.x * inv_det}}};
        result_di = {inv.m[0][0] * pijdi_small.x + inv.m[0][1] * pijdi_small.y,
                     inv.m[1][0] * pijdi_small.x + inv.m[1][1] * pijdi_small.y};
        result_dj = {inv.m[0][0] * pijdj_small.x + inv.m[0][1] * pijdj_small.y,
                     inv.m[1][0] * pijdj_small.x + inv.m[1][1] * pijdj_small.y};
    }

    // Apply textures
    for (const auto &texture : textures) {
        Vec3f texture_value = texture->GetColorAt(hit_tex_coords, hit_point, result_di, result_dj);

        if (texture->GetReplaceAllFlag()) {
            return texture_value;
        }

        FP_PRECISION diffuse_coeff = texture->GetDiffuseCoefficient();
        FP_PRECISION specular_coeff = texture->GetSpecularCoefficient();
        FP_PRECISION normal_coeff = texture->GetNormalCoefficient();
        FP_PRECISION bump_coeff = texture->GetBumpCoefficient();

        if (diffuse_coeff > 0.0f) {
            KD = KD * (1.0f - diffuse_coeff) + texture_value * diffuse_coeff;
        }
        if (specular_coeff > 0.0f) {
            KS = KS * (1.0f - specular_coeff) + texture_value * specular_coeff;
        }
        if (normal_coeff > 0.0f) {
            Vec3f texture_normal = normalize(texture_value * 2.0f - Vec3f{1.0f, 1.0f, 1.0f});
            hit_normal = normalize(tbn_matrix ^ texture_normal);
        } else if (bump_coeff > 0.0f) {
            FP_PRECISION center_value = (texture_value.x + texture_value.y + texture_value.z) / 3.0f;
            hit_point = hit_point + hit_normal * center_value * bump_coeff;
            Vec3f texture_bump_value_u, texture_bump_value_v;
            texture->GetGradientAt(hit_tex_coords, ray.origin_ + ray.direction_ * t_hit, hit_u_vector, hit_v_vector, 
                                   hit_tangent_vector, hit_bitangent_vector, texture_bump_value_u, texture_bump_value_v);
            FP_PRECISION u_value = (texture_bump_value_u.x + texture_bump_value_u.y + texture_bump_value_u.z) / 3.0f;
            FP_PRECISION v_value = (texture_bump_value_v.x + texture_bump_value_v.y + texture_bump_value_v.z) / 3.0f;
            Vec3f dqdu = hit_tangent_vector + (u_value * bump_coeff * hit_normal);
            Vec3f dqdv = hit_bitangent_vector + (v_value * bump_coeff * hit_normal);
            hit_normal = normalize(cross(dqdv, dqdu));
        }
    }

    Vec3f intersection_point = hit_point + hit_normal * shadow_ray_epsilon_;

    // STEP : REFLECTION AND REFRACTION (MIRROR, CONDUCTOR, DIELECTRIC)
    const bool can_recurse = settings.russian_roulette_enabled || (current_recursion < settings.max_recursion_depth);
    if (can_recurse) {
        Vec3f distorted_normal = hit_normal;
        if (material_ptr->roughness_ > 0.0f) {
            Vec3f u, v;
            BuildOrthonormalBasis(hit_normal, u, v);
            distorted_normal = FastNormalize(hit_normal + material_ptr->roughness_ *
                                (u * (FastRandom() - 0.5f) + v * (FastRandom() - 0.5f)));
        }

        MirrorMaterial *mirror_ptr = dynamic_cast<MirrorMaterial *>(material_ptr.get());
        ConductorMaterial *conductor_ptr = dynamic_cast<ConductorMaterial *>(material_ptr.get());
        DielectricMaterial *dielectric_ptr = dynamic_cast<DielectricMaterial *>(material_ptr.get());

        if (mirror_ptr || conductor_ptr || dielectric_ptr) {
            Vec3f reflection_direction = ray.direction_ - 2 * dot(ray.direction_, distorted_normal) * distorted_normal;
            Ray reflection_ray = {ray.pixel_, intersection_point, reflection_direction, ray.diff_, ray.time_};

            if (mirror_ptr) {
                Vec3f new_throughput = hadamard(throughput, mirror_ptr->mirror_);
                Vec3f reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr, 
                                                                            current_recursion, settings, new_throughput);
                total_light_value += hadamard(reflection_color, mirror_ptr->mirror_);
            } else if (conductor_ptr) {
                FP_PRECISION n2 = conductor_ptr->refraction_index_;
                FP_PRECISION k2 = conductor_ptr->absorption_index_;
                FP_PRECISION cos_theta = -dot(ray.direction_, distorted_normal);
                FP_PRECISION n2_k2_2 = n2 * n2 + k2 * k2;
                FP_PRECISION n2_cos_theta_tw = 2 * n2 * cos_theta;
                FP_PRECISION cos_theta_2 = cos_theta * cos_theta;
                FP_PRECISION rs = (n2_k2_2 - n2_cos_theta_tw + cos_theta_2) / (n2_k2_2 + n2_cos_theta_tw + cos_theta_2);
                FP_PRECISION rp = (n2_k2_2 * cos_theta_2 - n2_cos_theta_tw + 1) / (n2_k2_2 * cos_theta_2 + n2_cos_theta_tw + 1);
                FP_PRECISION fresnel = (rs + rp) * 0.5f;
                
                Vec3f conductor_factor = conductor_ptr->mirror_ * fresnel;
                Vec3f new_throughput = hadamard(throughput, conductor_factor);
                Vec3f reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr, 
                                                                            current_recursion, settings, new_throughput);
                total_light_value += hadamard(reflection_color, conductor_factor);
            } else {
                FP_PRECISION n1 = inside_object_ptr ? dielectric_ptr->refraction_index_ : 1.0f;
                FP_PRECISION n2 = inside_object_ptr ? 1.0f : dielectric_ptr->refraction_index_;
                FP_PRECISION cos_theta = dot(-ray.direction_, distorted_normal);
                FP_PRECISION cos_phi_2 = 1 - (n1 * n1 / (n2 * n2)) * (1 - cos_theta * cos_theta);
                
                if (cos_phi_2 > 0.0f) {
                    FP_PRECISION cos_phi = sqrt(cos_phi_2);
                    FP_PRECISION r_s = (n1 * cos_theta - n2 * cos_phi) / (n1 * cos_theta + n2 * cos_phi);
                    FP_PRECISION r_p = (n2 * cos_theta - n1 * cos_phi) / (n2 * cos_theta + n1 * cos_phi);
                    FP_PRECISION fresnel_r = (r_p * r_p + r_s * r_s) * 0.5f;
                    FP_PRECISION fresnel_t = 1.0f - fresnel_r;
                    
                    Vec3f refraction_direction = normalize((n1 / n2) * ray.direction_ + (n1 / n2 * cos_theta - cos_phi) * distorted_normal);
                    Ray refraction_ray = {ray.pixel_, intersection_point - 2 * shadow_ray_epsilon_ * distorted_normal, 
                                          refraction_direction, ray.diff_, ray.time_};
                    
                    Vec3f reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr, 
                                                current_recursion, settings, throughput * fresnel_r);
                    Vec3f refraction_color = RecursiveBRDFPathTracingAlgorithm(refraction_ray, 
                                                inside_object_ptr ? nullptr : hit_object_ptr,
                                                current_recursion, settings, throughput * fresnel_t);
                    
                    total_light_value += reflection_color * fresnel_r;
                    total_light_value += refraction_color * fresnel_t;
                } else {
                    Vec3f reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr, 
                                                                                current_recursion, settings, throughput);
                    total_light_value += reflection_color;
                }
            }
            return total_light_value;
        }
    }
                
    // STEP : HANDLE INSIDE HIT
    if (inside_object_ptr) {
        auto inside_casted = std::dynamic_pointer_cast<BaseObject>(inside_object_ptr);
        Vec3f absorption = dynamic_cast<DielectricMaterial *>(inside_casted->material_.get())->absorption_coefficient_;
        total_light_value.x *= exp(-absorption.x * t_hit);
        total_light_value.y *= exp(-absorption.y * t_hit);
        total_light_value.z *= exp(-absorption.z * t_hit);
        return total_light_value;
    }

    FP_PRECISION shadow_hit;
    Vec2f shadow_tex_coords;
    Vec3f shadow_normal, shadow_tangent, shadow_bitangent;
    Vec2f shadow_u, shadow_v;
    
    auto ShadowCheck = [&](const Vec3f& direction, FP_PRECISION max_dist) -> bool {
        Ray shadow_ray{ray.pixel_, intersection_point, direction, ray.diff_, ray.time_};
        shadow_hit = max_dist;
        return bvh_.Intersect(shadow_ray, objects_, shadow_hit, shadow_normal, shadow_tex_coords, 
                              shadow_u, shadow_v, shadow_tangent, shadow_bitangent, false, true) >= 0;
    };

    auto light_obj = std::dynamic_pointer_cast<ObjectLightSource>(hit_object_ptr);
    if (light_obj) {
        if (dot(ray.direction_, hit_normal) > 0) {
            return Vec3f{0, 0, 0};
        }
        return light_obj->radiance_;
    }

    if (spherical_directional_light_) {
        Vec3f direction;
        Vec3f env_radiance = spherical_directional_light_->GetIntensity(hit_normal, direction);
        if (!ShadowCheck(direction, std::numeric_limits<FP_PRECISION>::max())) {
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, direction, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(env_radiance, brdf) * std::max(0.0, dot(hit_normal, direction));
        }
    }
    
    for (const auto& point_light : point_lights_) {
        Vec3f light_dir = normalize(point_light->position_ - intersection_point);
        FP_PRECISION dist = norm(point_light->position_ - intersection_point);
        FP_PRECISION dist_sq = dist * dist;
        if (dist_sq < 1e-10) continue;
        if (!ShadowCheck(light_dir, dist)) {
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(point_light->intensity_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) / dist_sq;
        }
    }
    
    for (const auto& area_light : area_lights_) {
        std::vector<Vec2f> diff = area_light_sampling_algorithm_(1);
        Vec3f u, v;
        BuildOrthonormalBasis(FastNormalize(area_light->normal_), u, v);
        Vec3f light_pos = area_light->position_ + area_light->size_ * (u * (diff[0].x - 0.5f) + v * (diff[0].y - 0.5f));
        Vec3f light_dir = normalize(light_pos - intersection_point);
        FP_PRECISION dist = norm(light_pos - intersection_point);
        FP_PRECISION dist_sq = dist * dist;
        if (dist_sq < 1e-10) continue;
        if (!ShadowCheck(light_dir, dist)) {
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            Vec3f light_normal = FastNormalize(area_light->normal_);
            total_light_value += hadamard(area_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) * 
                                 area_light->size_ * area_light->size_ * dot(-light_normal, light_dir) / dist_sq;
        }
    }
    
    for (const auto& spot_light : spot_lights_) {
        Vec3f dir_from_light = normalize(intersection_point - spot_light->position_);
        FP_PRECISION dist = norm(spot_light->position_ - intersection_point);
        FP_PRECISION angle = acos(dot(dir_from_light, normalize(spot_light->direction_)));
        FP_PRECISION coverage_rad = spot_light->coverage_angle_ * M_PI / 180.0f;
        FP_PRECISION falloff_rad = spot_light->falloff_angle_ * M_PI / 180.0f;
        
        if (angle <= coverage_rad * 0.5f) {
            Vec3f light_dir = normalize(spot_light->position_ - intersection_point);
            if (!ShadowCheck(light_dir, dist)) {
                FP_PRECISION attenuation = 1.0f;
                if (angle > falloff_rad * 0.5f) {
                    FP_PRECISION coeff = (cos(angle) - cos(coverage_rad * 0.5f)) / (cos(falloff_rad * 0.5f) - cos(coverage_rad * 0.5f));
                    attenuation = coeff * coeff * coeff * coeff;
                }
                Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                            material_ptr->refraction_index_, material_ptr->absorption_index_);
                total_light_value += hadamard(spot_light->intensity_ * attenuation / (dist * dist), brdf) * 
                                     std::max(0.0, dot(hit_normal, light_dir));
            }
        }
    }
    
    for (const auto& dir_light : directional_lights_) {
        Vec3f light_dir = -normalize(dir_light->direction_);
        if (!ShadowCheck(light_dir, std::numeric_limits<FP_PRECISION>::max())) {
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(dir_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir));
        }
    }

    // PATH TRACING - Indirect illumination
    if (can_recurse) {
        Vec3f tangent, bitangent;
        BuildOrthonormalBasis(hit_normal, tangent, bitangent);
        
        for (int i = 0; i < sample_count; i++) {
            FP_PRECISION theta, phi, indirect_pdf;
            
            if (settings.importance_sampling_enabled) {
                cosine_hemisphere_sample(theta, phi, indirect_pdf);
            } else {
                uniform_hemisphere_sample(theta, phi, indirect_pdf);
            }

            FP_PRECISION sin_theta = std::sin(theta);
            FP_PRECISION cos_theta = std::cos(theta);
            Vec3f sample_dir = FastNormalize(tangent * (sin_theta * std::cos(phi)) + 
                                              bitangent * (sin_theta * std::sin(phi)) + 
                                              hit_normal * cos_theta);
            Ray sample_ray = {ray.pixel_, intersection_point, sample_dir, ray.diff_, ray.time_};
            
            Vec3f brdf = material_ptr->brdf_->Evaluate(-ray.direction_, sample_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            FP_PRECISION cos_term = std::max(static_cast<FP_PRECISION>(0.0), dot(hit_normal, sample_dir));
            
            if (!std::isfinite(indirect_pdf) || indirect_pdf <= 1e-6) {
                continue;
            }
            
            Vec3f new_throughput = hadamard(throughput, brdf) * cos_term / indirect_pdf;
            
            Vec3f sample_color = RecursiveBRDFPathTracingAlgorithm(sample_ray, inside_object_ptr, 
                                                                    current_recursion, settings, new_throughput);
            Vec3f indirect_value = hadamard(sample_color, brdf) * cos_term / indirect_pdf;
            
            if (!std::isfinite(indirect_value.x)) indirect_value.x = 0;
            if (!std::isfinite(indirect_value.y)) indirect_value.y = 0;
            if (!std::isfinite(indirect_value.z)) indirect_value.z = 0;
            
            if (settings.sample_max_val > 0) {
                indirect_value.x = std::min(indirect_value.x, settings.sample_max_val);
                indirect_value.y = std::min(indirect_value.y, settings.sample_max_val);
                indirect_value.z = std::min(indirect_value.z, settings.sample_max_val);
            }

            Vec3f direct_value = {0, 0, 0};
            if (settings.nee_enabled && !light_objects_.empty()) {
                int light_idx = FastRandomInteger(light_objects_.size());
                auto light_obj_casted = std::dynamic_pointer_cast<ObjectLightSource>(light_objects_[light_idx]);
                if (light_obj_casted) {
                    Vec3f sample_point, sample_normal;
                    FP_PRECISION light_pdf;
                    
                    light_obj_casted->Sample(intersection_point, sample_point, sample_normal, light_pdf);
                    light_pdf /= static_cast<FP_PRECISION>(light_objects_.size());
                    
                    if (std::isfinite(light_pdf) && light_pdf > 1e-6) {
                        Vec3f light_dir = normalize(sample_point - intersection_point);
                        FP_PRECISION light_dist = norm(sample_point - intersection_point);
                        if (!ShadowCheck(light_dir, light_dist)) {
                            Vec3f light_brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                                              material_ptr->refraction_index_, material_ptr->absorption_index_);
                            direct_value = hadamard(light_obj_casted->radiance_, light_brdf) * 
                                           std::max(0.0, dot(hit_normal, light_dir)) / light_pdf;
                        }
                    }
                }
            }
            
            if (!std::isfinite(direct_value.x)) direct_value.x = 0;
            if (!std::isfinite(direct_value.y)) direct_value.y = 0;
            if (!std::isfinite(direct_value.z)) direct_value.z = 0;
            
            if (settings.sample_max_val > 0) {
                direct_value.x = std::min(direct_value.x, settings.sample_max_val);
                direct_value.y = std::min(direct_value.y, settings.sample_max_val);
                direct_value.z = std::min(direct_value.z, settings.sample_max_val);
            }

            if (settings.mis_balance_enabled) {
                FP_PRECISION light_hit = std::numeric_limits<FP_PRECISION>::max();
                Vec3f light_normal;
                Vec2f light_tex;
                Vec3f light_tan, light_bitan;
                Vec2f light_uv_u, light_uv_v;
                std::shared_ptr<BaseObject> light_hit_obj = nullptr;
                
                for (const auto& light_object : light_objects_) {
                    FP_PRECISION temp_hit = std::numeric_limits<FP_PRECISION>::max();
                    Vec3f temp_normal;
                    if (light_object->Intersect(sample_ray, temp_hit, temp_normal, light_tex, light_uv_u, light_uv_v, 
                                                light_tan, light_bitan, false) && light_hit > temp_hit) {
                        light_hit = temp_hit;
                        light_hit_obj = light_object;
                        light_normal = temp_normal;
                    }
                }
                
                bool hit_light_frontface = light_hit_obj && !ShadowCheck(sample_dir, light_hit);
                
                if (hit_light_frontface) {
                    auto hit_light = std::dynamic_pointer_cast<ObjectLightSource>(light_hit_obj);
                    if (hit_light) {
                        Vec3f sp, sn;
                        FP_PRECISION light_pdf;
                        hit_light->Sample(intersection_point, sp, sn, light_pdf);
                        
                        FP_PRECISION weight_sum = indirect_pdf + light_pdf;
                        if (std::isfinite(weight_sum) && weight_sum > 1e-6) {
                            total_light_value += indirect_value * (indirect_pdf / weight_sum);
                            total_light_value += direct_value * (light_pdf / weight_sum);
                        } else {
                            total_light_value += indirect_value;
                            total_light_value += direct_value;
                        }
                    } else {
                        total_light_value += indirect_value;
                        total_light_value += direct_value;
                    }
                } else {
                    total_light_value += indirect_value;
                    total_light_value += direct_value;
                }
            } else {
                total_light_value += indirect_value;
                total_light_value += direct_value;
            }
        }
    }

    Vec3f result = total_light_value / static_cast<FP_PRECISION>(sample_count);
    
    if (!std::isfinite(result.x)) result.x = 0;
    if (!std::isfinite(result.y)) result.y = 0;
    if (!std::isfinite(result.z)) result.z = 0;
    
    return result;
}
