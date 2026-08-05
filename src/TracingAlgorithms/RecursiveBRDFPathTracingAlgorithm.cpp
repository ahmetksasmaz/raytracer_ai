#include <cmath>
#include <limits>

#include "Scene.hpp"

namespace {

// Beer-Lambert attenuation over a segment travelled inside a dielectric.
Spectrum ApplyBeerLambert(const Spectrum &radiance,
                          const std::shared_ptr<BaseObject> &medium,
                          FP_PRECISION distance) {
    auto *dielectric = dynamic_cast<DielectricMaterial *>(medium->material_.get());
    if (!dielectric) return radiance;

    const Spectrum sigma = dielectric->absorption_coefficient_;
    // The parser stores -1 when AbsorptionCoefficient is absent. Feeding that
    // to exp(-sigma * d) would AMPLIFY the radiance exponentially, so treat any
    // negative band as "no absorption specified".
    for (int i = 0; i < kSpectralBands; i++) {
        if (sigma[i] < 0.0) return radiance;
    }

    // Absorption is now wavelength-dependent, which is the whole point: coloured
    // glass attenuates each band differently rather than each RGB channel.
    Spectrum result;
    for (int i = 0; i < kSpectralBands; i++) {
        result[i] = radiance[i] * std::exp(-sigma[i] * distance);
    }
    return result;
}

}  // namespace

Spectrum Scene::RecursiveBRDFPathTracingAlgorithm(
    Ray &ray,
    const std::shared_ptr<BaseObject> inside_object_ptr,
    int current_recursion, const PathTracerSettings& settings, const PathState& state)
{
    current_recursion++;

    // Two accumulators, deliberately kept apart. Analytic lights (point, area,
    // spot, directional, environment) are gathered exactly once per vertex,
    // while indirect light and next-event estimation are gathered once per
    // splitting sample. Only the second is divided by sample_count at the end --
    // dividing a single analytic evaluation by the splitting factor is what made
    // SplittingFactor darken direct lighting in proportion to itself.
    Spectrum analytic_light_value;
    Spectrum sampled_light_value;

    const int sample_count = (current_recursion == 1) ? settings.splitting_factor : 1;

    const int HARD_MAX_DEPTH = 32;
    if (current_recursion > HARD_MAX_DEPTH) {
        return Spectrum();
    }

    // Russian roulette. Killing a path with probability 1-p is only unbiased if
    // the survivors are scaled by 1/p to carry the energy of the ones that were
    // killed. Without that compensation the estimator loses energy at every
    // bounce and the whole image drifts dark.
    FP_PRECISION rr_scale = 1.0;
    if (settings.russian_roulette_enabled && current_recursion > settings.min_recursion_depth) {
        const FP_PRECISION max_throughput = state.throughput.MaxComponent();
        if (!(max_throughput > 0.0)) {
            return Spectrum();  // path carries no energy; nothing to estimate
        }
        // Floor the probability so 1/p cannot explode on a near-black path.
        const FP_PRECISION rr_probability =
            std::min(std::max(max_throughput, static_cast<FP_PRECISION>(0.05)),
                     static_cast<FP_PRECISION>(0.95));
        if (FastRandom() > rr_probability) {
            return Spectrum();
        }
        rr_scale = 1.0 / rr_probability;
    }

    // Throughput as seen by any child of this vertex, already carrying the
    // roulette compensation.
    const Spectrum throughput = state.throughput * rr_scale;

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
        if (current_recursion == 1) {
            if (background_texture_map_) {
                FP_PRECISION u = 0.5 + (atan2(ray.direction_.z, ray.direction_.x) / (2 * M_PI));
                FP_PRECISION v = 0.5 - (asin(ray.direction_.y) / M_PI);
                return UpliftRGB(background_texture_map_->GetColorAt({u, v}, {0,0,0}));
            }
            return background_color_;
        }
        return Spectrum();
    }

    // STEP : EMITTED RADIANCE
    // An emitter terminates the path. Whether this path is entitled to add the
    // emission depends on whether some other strategy already accounted for it.
    {
        auto emitter = std::dynamic_pointer_cast<ObjectLightSource>(hit_object_ptr);
        if (emitter) {
            // Emitters are one-sided; the back face radiates nothing.
            if (dot(ray.direction_, hit_normal) > 0) {
                return Spectrum();
            }

            // Either nothing else is sampling lights, or this ray arrived
            // through a specular bounce -- next-event estimation cannot aim
            // through a delta reflection, so no one else can have counted it.
            if (!settings.nee_enabled || state.prev_specular) {
                return emitter->radiance_ * rr_scale;
            }

            if (settings.mis_balance_enabled) {
                // Balance heuristic for the BSDF-sampling strategy. Both pdfs
                // describe THIS direction: prev_bsdf_pdf is the density with
                // which the previous vertex generated this very ray, and
                // light_pdf is the density with which light sampling would have
                // produced the same direction from that same vertex
                // (ray.origin_ IS the previous shading point).
                const Vec3f emitter_point = ray.origin_ + ray.direction_ * t_hit;
                const FP_PRECISION light_pdf = LightSamplingPdf(
                    hit_object_ptr, ray.origin_, emitter_point, hit_normal);
                const FP_PRECISION denominator = state.prev_bsdf_pdf + light_pdf;
                const FP_PRECISION weight =
                    denominator > 1e-9 ? state.prev_bsdf_pdf / denominator : 1.0;
                return emitter->radiance_ * weight * rr_scale;
            }

            // NEE alone owns direct lighting. Adding emission here as well is
            // exactly the double count that makes NEE renders read ~2x bright.
            return Spectrum();
        }
    }

    // Shade double-sided. A triangle wound away from the ray would otherwise
    // have every cosine term evaluate to zero and render black, which is a
    // constant hazard with imported geometry. Emitters are excluded on purpose:
    // one-sidedness is meaningful for them, and the NEE and MIS pdfs are built
    // on their geometric orientation.
    if (dot(ray.direction_, hit_normal) > 0 &&
        !std::dynamic_pointer_cast<ObjectLightSource>(hit_object_ptr)) {
        hit_normal = -hit_normal;
    }

    // STEP : FIND PROPERTIES OF OBJECT AND APPLY TEXTURES
    std::shared_ptr<BaseMaterial> material_ptr = hit_object_ptr->material_;
    const std::vector<std::shared_ptr<BaseTextureMap>>& textures = hit_object_ptr->textures_;

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

    // Bump and normal mapping below replace hit_normal outright. Keep the
    // geometric normal so the perturbed one can be checked against it.
    const Vec3f geometric_normal = hit_normal;

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

    // A perturbed shading normal must stay on the same side as the surface it
    // belongs to. Bump mapping builds its normal from a cross product whose
    // handedness depends on the tangent frame, and normal mapping goes through
    // a TBN basis that can be mirrored, so either can come out inverted. When
    // that happens every cosine term evaluates to zero and the surface renders
    // black -- which is exactly what the bump-mapped scenes did.
    if (dot(hit_normal, geometric_normal) < 0.0) {
        hit_normal = -hit_normal;
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
            // Every branch below is a delta (specular) scattering event, so
            // children are told prev_specular = true: next-event estimation
            // cannot sample a light through a mirror direction, which means the
            // BSDF path must be allowed to pick up emission on its own.
            Spectrum specular_value;
            Vec3f reflection_direction = ray.direction_ - 2 * dot(ray.direction_, distorted_normal) * distorted_normal;
            Ray reflection_ray = {ray.pixel_, intersection_point, reflection_direction, ray.diff_, ray.time_};

            if (mirror_ptr) {
                Spectrum new_throughput = hadamard(throughput, mirror_ptr->mirror_);
                Spectrum reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr,
                                                                            current_recursion, settings,
                                                                            PathState{new_throughput, 0.0, true});
                specular_value += hadamard(reflection_color, mirror_ptr->mirror_);
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
                
                Spectrum conductor_factor = conductor_ptr->mirror_ * fresnel;
                Spectrum new_throughput = hadamard(throughput, conductor_factor);
                Spectrum reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr,
                                                                            current_recursion, settings,
                                                                            PathState{new_throughput, 0.0, true});
                specular_value += hadamard(reflection_color, conductor_factor);
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
                    
                    Spectrum reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr,
                                                current_recursion, settings,
                                                PathState{throughput * fresnel_r, 0.0, true});
                    Spectrum refraction_color = RecursiveBRDFPathTracingAlgorithm(refraction_ray,
                                                inside_object_ptr ? nullptr : hit_object_ptr,
                                                current_recursion, settings,
                                                PathState{throughput * fresnel_t, 0.0, true});

                    specular_value += reflection_color * fresnel_r;
                    specular_value += refraction_color * fresnel_t;
                } else {
                    Spectrum reflection_color = RecursiveBRDFPathTracingAlgorithm(reflection_ray, inside_object_ptr,
                                                                                current_recursion, settings,
                                                                                PathState{throughput, 0.0, true});
                    specular_value += reflection_color;
                }
            }

            // Beer-Lambert over the segment just travelled through the medium.
            // This MUST happen before returning: previously the absorption code
            // sat after this unconditional return, so it only ever ran once
            // recursion was exhausted and the value it scaled was already zero.
            // AbsorptionCoefficient therefore did nothing at all.
            if (inside_object_ptr) {
                specular_value = ApplyBeerLambert(specular_value, inside_object_ptr, t_hit);
            }
            return specular_value * rr_scale;
        }
    }

    // STEP : HANDLE INSIDE HIT
    // Reached only when recursion is exhausted inside a dielectric.
    if (inside_object_ptr) {
        return ApplyBeerLambert(analytic_light_value, inside_object_ptr, t_hit) * rr_scale;
    }

    FP_PRECISION shadow_hit;
    Vec2f shadow_tex_coords;
    Vec3f shadow_normal, shadow_tangent, shadow_bitangent;
    Vec2f shadow_u, shadow_v;
    
    // max_dist is shortened slightly by callers that aim at a light so the light
    // itself is not treated as its own occluder.
    auto ShadowCheck = [&](const Vec3f& direction, FP_PRECISION max_dist) -> bool {
        Ray shadow_ray{ray.pixel_, intersection_point, direction, ray.diff_, ray.time_};
        shadow_hit = max_dist;
        return IntersectScene(shadow_ray, shadow_hit, shadow_normal, shadow_tex_coords,
                              shadow_u, shadow_v, shadow_tangent, shadow_bitangent,
                              true) != nullptr;
    };

    // (Emitted radiance is handled up front, before shading -- see STEP :
    // EMITTED RADIANCE. It has to be gated on how the path arrived here, which
    // is information only the caller has.)

    if (spherical_directional_light_) {
        Vec3f direction;
        Spectrum env_radiance = UpliftRGB(spherical_directional_light_->GetIntensity(hit_normal, direction));
        if (!ShadowCheck(direction, std::numeric_limits<FP_PRECISION>::max())) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, direction, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            analytic_light_value += hadamard(env_radiance, brdf) * std::max(0.0, dot(hit_normal, direction));
        }
    }
    
    for (const auto& point_light : point_lights_) {
        Vec3f light_dir = normalize(point_light->position_ - intersection_point);
        FP_PRECISION dist = norm(point_light->position_ - intersection_point);
        FP_PRECISION dist_sq = dist * dist;
        if (dist_sq < 1e-10) continue;
        if (!ShadowCheck(light_dir, dist)) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            analytic_light_value += hadamard(point_light->intensity_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) / dist_sq;
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
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            Vec3f light_normal = FastNormalize(area_light->normal_);
            // Clamp the light-side cosine too -- see the ray tracer for why.
            analytic_light_value += hadamard(area_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir)) *
                                 area_light->size_ * area_light->size_ * std::max(0.0, dot(-light_normal, light_dir)) / dist_sq;
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
                analytic_light_value += hadamard(spot_light->intensity_ * attenuation / (dist * dist), brdf) * 
                                     std::max(0.0, dot(hit_normal, light_dir));
            }
        }
    }
    
    for (const auto& dir_light : directional_lights_) {
        Vec3f light_dir = -normalize(dir_light->direction_);
        if (!ShadowCheck(light_dir, std::numeric_limits<FP_PRECISION>::max())) {
            Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS, 
                                                        material_ptr->refraction_index_, material_ptr->absorption_index_);
            analytic_light_value += hadamard(dir_light->radiance_, brdf) * std::max(0.0, dot(hit_normal, light_dir));
        }
    }

    // STEP : INDIRECT ILLUMINATION AND NEXT-EVENT ESTIMATION
    //
    // Each iteration draws one BSDF direction and, when NEE is on, one explicit
    // light sample. The two are separate estimators of the same quantity, so
    // when MIS is enabled each is weighted by the balance heuristic; when it is
    // not, exactly one of them is allowed to carry direct lighting (NEE if
    // present, otherwise the BSDF path picking up emission at the emitter).
    if (can_recurse) {
        Vec3f tangent, bitangent;
        BuildOrthonormalBasis(hit_normal, tangent, bitangent);

        for (int i = 0; i < sample_count; i++) {
            // --- BSDF / hemisphere strategy ---------------------------------
            FP_PRECISION theta, phi, indirect_pdf;
            if (settings.importance_sampling_enabled) {
                cosine_hemisphere_sample(theta, phi, indirect_pdf);
            } else {
                uniform_hemisphere_sample(theta, phi, indirect_pdf);
            }

            if (!std::isfinite(indirect_pdf) || indirect_pdf <= 1e-9) {
                continue;
            }

            const FP_PRECISION sin_theta = std::sin(theta);
            const FP_PRECISION cos_theta = std::cos(theta);
            const Vec3f sample_dir = FastNormalize(tangent * (sin_theta * std::cos(phi)) +
                                                   bitangent * (sin_theta * std::sin(phi)) +
                                                   hit_normal * cos_theta);
            Ray sample_ray = {ray.pixel_, intersection_point, sample_dir, ray.diff_, ray.time_};

            const Spectrum brdf = material_ptr->brdf_->Evaluate(-ray.direction_, sample_dir, hit_normal, KD, KS,
                                                             material_ptr->refraction_index_, material_ptr->absorption_index_);
            const FP_PRECISION cos_term = std::max(static_cast<FP_PRECISION>(0.0), dot(hit_normal, sample_dir));

            // The child needs the pdf that produced its ray so that, if it lands
            // on an emitter, it can form the MIS weight there. This is why the
            // weight is applied at the emitter and not here: applying it to the
            // whole recursive return would also scale the indirect light coming
            // back from deeper bounces, which MIS has no business touching.
            const Spectrum new_throughput = hadamard(throughput, brdf) * cos_term / indirect_pdf;
            const PathState child_state{new_throughput, indirect_pdf, false};

            Spectrum sample_color = RecursiveBRDFPathTracingAlgorithm(sample_ray, inside_object_ptr,
                                                                   current_recursion, settings, child_state);
            Spectrum indirect_value = hadamard(sample_color, brdf) * cos_term / indirect_pdf;

            indirect_value.SanitizeInPlace();

            indirect_value.ClampMaxInPlace(settings.sample_max_val);

            sampled_light_value += indirect_value;

            // --- Light-sampling strategy (next-event estimation) ------------
            if (!settings.nee_enabled || light_objects_.empty()) {
                continue;
            }

            const int light_index = FastRandomInteger(static_cast<int>(light_objects_.size()));
            const std::shared_ptr<BaseObject> &light_object = light_objects_[light_index];
            auto emitter = std::dynamic_pointer_cast<ObjectLightSource>(light_object);
            if (!emitter) continue;

            Vec3f light_point, light_normal;
            FP_PRECISION emitter_pdf = 0.0;
            emitter->Sample(intersection_point, light_point, light_normal, emitter_pdf);
            if (!std::isfinite(emitter_pdf) || emitter_pdf <= 1e-9) continue;

            // Fold in the 1/N chance of having chosen this emitter. The MIS
            // weight below must see the SAME combined pdf, which is why both go
            // through LightSamplingPdf rather than applying 1/N by hand here.
            const FP_PRECISION light_pdf =
                emitter_pdf / static_cast<FP_PRECISION>(light_objects_.size());

            const Vec3f light_dir = normalize(light_point - intersection_point);
            const FP_PRECISION light_dist = norm(light_point - intersection_point);
            // Shorten slightly: the emitter is in the BVH, so a shadow ray aimed
            // exactly at it can register the light as its own occluder and
            // speckle the result.
            if (ShadowCheck(light_dir, light_dist * (1.0 - 1e-4))) continue;

            const Spectrum light_brdf = material_ptr->brdf_->Evaluate(-ray.direction_, light_dir, hit_normal, KD, KS,
                                                                   material_ptr->refraction_index_, material_ptr->absorption_index_);
            const FP_PRECISION light_cos = std::max(static_cast<FP_PRECISION>(0.0), dot(hit_normal, light_dir));
            Spectrum direct_value = hadamard(emitter->radiance_, light_brdf) * light_cos / light_pdf;

            if (settings.mis_balance_enabled) {
                // Balance heuristic for the light-sampling strategy, evaluated
                // for the direction that was actually sampled here.
                const FP_PRECISION bsdf_pdf_for_light_dir =
                    hemisphere_pdf(light_cos, settings.importance_sampling_enabled);
                const FP_PRECISION denominator = light_pdf + bsdf_pdf_for_light_dir;
                direct_value = denominator > 1e-9
                                   ? direct_value * (light_pdf / denominator)
                                   : direct_value;
            }

            direct_value.SanitizeInPlace();

            direct_value.ClampMaxInPlace(settings.sample_max_val);

            sampled_light_value += direct_value;
        }
    }

    // Analytic lights were evaluated once; the sampled terms were evaluated
    // sample_count times. Only the latter is averaged.
    Spectrum result = analytic_light_value +
                   sampled_light_value / static_cast<FP_PRECISION>(sample_count);

    result = result * rr_scale;

    result.SanitizeInPlace();

    return result;
}
