#include <cmath>
#include <limits>

#include "Scene.hpp"

Spectrum Scene::RecursiveBRDFRayTracingAlgorithm(
    Ray &ray,
    const std::shared_ptr<BaseObject> inside_object_ptr,
    int remaining_recursion, int max_recursion)
{
    remaining_recursion--;
    Spectrum total_light_value;

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
        hit_object_ptr = IntersectScene(ray, t_hit, hit_normal, hit_tex_coords,
                                        hit_u_vector, hit_v_vector,
                                        hit_tangent_vector, hit_bitangent_vector);
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
            return UpliftRGB(spherical_directional_light_->GetIntensity(ray.direction_, direction, true));
        }
        if (remaining_recursion == max_recursion - 1) {
            if (background_texture_map_) {
                FP_PRECISION u = 0.5 + (atan2(ray.direction_.z, ray.direction_.x) / (2 * M_PI));
                FP_PRECISION v = 0.5 - (asin(ray.direction_.y) / M_PI);
                return UpliftRGB(background_texture_map_->GetColorAt({u, v}, {0,0,0}));
            }
            return background_color_;
        }
        return Spectrum();
    }

    // Shade double-sided -- see the path tracer for the rationale. Emitters keep
    // their geometric orientation.
    if (dot(ray.direction_, hit_normal) > 0 &&
        !std::dynamic_pointer_cast<ObjectLightSource>(hit_object_ptr)) {
        hit_normal = -hit_normal;
    }

    // STEP : FIND PROPERTIES OF OBJECT AND APPLY TEXTURES

    std::shared_ptr<BaseMaterial> material_ptr = hit_object_ptr->material_;
    const std::vector<std::shared_ptr<BaseTextureMap>>& textures = hit_object_ptr->textures_;

    Spectrum KA = material_ptr->ambient_;
    Spectrum KD = material_ptr->diffuse_;
    Spectrum KS = material_ptr->specular_;

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
            return UpliftRGB(texture_value);
        }

        FP_PRECISION diffuse_coeff = texture->GetDiffuseCoefficient();
        FP_PRECISION specular_coeff = texture->GetSpecularCoefficient();
        FP_PRECISION normal_coeff = texture->GetNormalCoefficient();
        FP_PRECISION bump_coeff = texture->GetBumpCoefficient();

        if (diffuse_coeff > 0.0f) {
            KD = KD * (1.0 - diffuse_coeff) + UpliftRGB(texture_value) * diffuse_coeff;
        }
        if (specular_coeff > 0.0f) {
            KS = KS * (1.0 - specular_coeff) + UpliftRGB(texture_value) * specular_coeff;
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
    if (remaining_recursion > 0) {
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
            Spectrum reflection_color = RecursiveBRDFRayTracingAlgorithm(reflection_ray, inside_object_ptr, remaining_recursion, max_recursion);

            if (mirror_ptr) {
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
                
                total_light_value += hadamard(reflection_color, conductor_ptr->mirror_ * fresnel);
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
                    
                    Spectrum refraction_color = RecursiveBRDFRayTracingAlgorithm(
                        refraction_ray, inside_object_ptr ? nullptr : hit_object_ptr,
                        remaining_recursion, max_recursion);
                    
                    total_light_value += reflection_color * fresnel_r;
                    total_light_value += refraction_color * fresnel_t;
                } else {
                    total_light_value += reflection_color;
                }
            }
        }
    }

    // STEP : HANDLE INSIDE HIT
    if (inside_object_ptr) {
        auto inside_casted = std::dynamic_pointer_cast<BaseObject>(inside_object_ptr);
        const Spectrum absorption = dynamic_cast<DielectricMaterial *>(inside_casted->material_.get())->absorption_coefficient_;
        for (int band = 0; band < kSpectralBands; band++) {
            total_light_value[band] *= std::exp(-absorption[band] * t_hit);
        }
        return total_light_value;
    }

    FP_PRECISION shadow_hit;
    Vec2f shadow_tex_coords;
    Vec3f shadow_normal, shadow_tangent, shadow_bitangent;
    Vec2f shadow_u, shadow_v;
    
    auto ShadowCheck = [&](const Vec3f& direction, FP_PRECISION max_dist) -> bool {
        Ray shadow_ray{ray.pixel_, intersection_point, direction, ray.diff_, ray.time_};
        shadow_hit = max_dist;
        return IntersectScene(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords,
                              shadow_u, shadow_v, shadow_tangent, shadow_bitangent,
                              true) != nullptr;
    };

    auto light_obj = std::dynamic_pointer_cast<ObjectLightSource>(hit_object_ptr);
    if (light_obj) {
        return light_obj->radiance_;
    }

    if (spherical_directional_light_) {
        Vec3f direction;
        Spectrum env_radiance = UpliftRGB(spherical_directional_light_->GetIntensity(hit_normal, direction));
        if (!ShadowCheck(direction, std::numeric_limits<FP_PRECISION>::max())) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, direction, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(env_radiance, brdf) * std::max(0.0, dot(hit_normal, direction));
        }
    }
    
    for (const auto& point_light : point_lights_) {
        Vec3f light_dir = normalize(point_light->position_ - intersection_point);
        FP_PRECISION dist = norm(point_light->position_ - intersection_point);
        if (!ShadowCheck(light_dir, dist)) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(point_light->intensity_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) / (dist * dist);
        }
    }
    
    for (const auto& area_light : area_lights_) {
        std::vector<Vec2f> diff = area_light_sampling_algorithm_(1);
        Vec3f u, v;
        BuildOrthonormalBasis(FastNormalize(area_light->normal_), u, v);
        Vec3f light_pos = area_light->position_ + area_light->size_ * (u * (diff[0].x - 0.5f) + v * (diff[0].y - 0.5f));
        Vec3f light_dir = normalize(light_pos - intersection_point);
        FP_PRECISION dist = norm(light_pos - intersection_point);
        if (!ShadowCheck(light_dir, dist)) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            Vec3f light_normal = FastNormalize(area_light->normal_);
            // The light-side cosine needs the same clamp as the surface-side
            // one. Unclamped, a surface behind the emitting face receives a
            // NEGATIVE contribution rather than none at all.
            total_light_value += hadamard(area_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) *
                                 area_light->size_ * area_light->size_ * std::max(0.0, dot(-light_normal, light_dir)) / (dist * dist);
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
                Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                            material_ptr->refraction_index_, material_ptr->absorption_index_);
                total_light_value += hadamard(spot_light->intensity_ * attenuation / (dist * dist), brdf) * 
                                     std::max(0.0, dot(hit_normal, light_dir));
            }
        }
    }
    
    for (const auto& dir_light : directional_lights_) {
        Vec3f light_dir = -normalize(dir_light->direction_);
        if (!ShadowCheck(light_dir, std::numeric_limits<FP_PRECISION>::max())) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            total_light_value += hadamard(dir_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir));
        }
    }
    
    // STEP : OBJECT LIGHT SAMPLING
    {
        int object_light_count = light_objects_.size();
        if (object_light_count > 0) {
            int random_light_index = FastRandomInteger(object_light_count);
            auto light_object = light_objects_[random_light_index];
            auto light_object_casted = std::dynamic_pointer_cast<ObjectLightSource>(light_object);
            Vec3f sample_point;
            Vec3f sample_normal;
            FP_PRECISION pdf_value = 0.0;

            light_object_casted->Sample(intersection_point, sample_point, sample_normal, pdf_value);
            if (std::isfinite(pdf_value) && pdf_value > 1e-6) {
                pdf_value /= static_cast<FP_PRECISION>(object_light_count);
                Vec3f light_dir = normalize(sample_point - intersection_point);
                FP_PRECISION dist = norm(sample_point - intersection_point);
                // Shorten slightly so the emitter is not its own occluder.
                if (!ShadowCheck(light_dir, dist * (1.0 - 1e-4))) {
                    Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                                material_ptr->refraction_index_, material_ptr->absorption_index_);
                    total_light_value += hadamard(light_object_casted->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) / pdf_value;
                }
            }
        }
    }

    total_light_value += hadamard(ambient_light_->intensity_, KA);

    return total_light_value;
};
