#pragma once

#include "../extern/parser.h"
#include "Helper.hpp"
#include "BaseBRDF.hpp"
#include <cmath>
#include <algorithm>

class TorranceSparrow : public BaseBRDF {
 public:
    TorranceSparrow(const FP_PRECISION exponent, const bool kd_fresnel)
        : BaseBRDF(exponent, false), kd_fresnel_(kd_fresnel) {}
    Vec3f Evaluate(const Vec3f &ray_coming_direction, const Vec3f &light_coming_direction, const Vec3f &normal, const Vec3f &kd, const Vec3f &ks, FP_PRECISION refraction_index, FP_PRECISION absorption_index) const override {
        Vec3f half_vector = normalize(ray_coming_direction + light_coming_direction);
        FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(normal, half_vector));
        FP_PRECISION d_alpha = ((exponent_+2.0) * std::pow(NdotH, exponent_)) /(2.0 * M_PI);
        FP_PRECISION g_value = std::min((FP_PRECISION)1.0, std::min((FP_PRECISION)(2.0 * dot(normal, half_vector) * dot(normal, light_coming_direction) / dot(light_coming_direction, half_vector)),
                                                (FP_PRECISION)(2.0 * dot(normal, half_vector) * dot(normal, ray_coming_direction) / dot(light_coming_direction, half_vector))));
        FP_PRECISION f_zero_value = 1.0;
        if(refraction_index > 0.0 && absorption_index < 0.0){
            FP_PRECISION n1 = refraction_index;
            FP_PRECISION n2 = 1.0;
            f_zero_value = std::pow((n1 - n2) / (n1 + n2), 2.0);
        }
        else if(refraction_index > 0.0 && absorption_index > 0.0){
            FP_PRECISION n1 = refraction_index;
            FP_PRECISION n2 = 1.0;
            FP_PRECISION k2 = absorption_index;
            f_zero_value = ((n1 - n2)*(n1 - n2) + k2*k2) / ((n1 + n2)*(n1 + n2) + k2*k2);
        }
        FP_PRECISION f_theta_value = f_zero_value + (1.0 - f_zero_value) * std::pow((1.0 - dot(half_vector, ray_coming_direction)), 5.0);
        FP_PRECISION specular_term = (d_alpha * g_value * f_theta_value) / (4.0 * dot(ray_coming_direction, normal) * dot(light_coming_direction, normal));
        if(kd_fresnel_){
            return kd * (1.0 - f_theta_value) * (1.0 / M_PI) + ks * specular_term;
        }
        else{
            return kd * (1.0 / M_PI) + ks * specular_term;
        }
    }
    ~TorranceSparrow() override = default;
private:
    const bool kd_fresnel_;
};