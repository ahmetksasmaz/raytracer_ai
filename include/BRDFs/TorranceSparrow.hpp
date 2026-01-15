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
    Vec3f Evaluate(const Vec3f &incoming_direction, const Vec3f &outgoing_direction, const Vec3f &normal, const Vec3f &kd, const Vec3f &ks, FP_PRECISION f_theta_value) const override {
        Vec3f half_vector = normalize(incoming_direction + outgoing_direction);
        FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(normal, half_vector));
        FP_PRECISION d_alpha = (exponent_+2.0)/(2.0 * M_PI) * std::pow(NdotH, exponent_);
        FP_PRECISION g_value = std::min((FP_PRECISION)1.0, std::min((FP_PRECISION)(2.0 * dot(normal, half_vector) * dot(normal, outgoing_direction) / dot(outgoing_direction, half_vector)),
                                                (FP_PRECISION)(2.0 * dot(normal, half_vector) * dot(normal, incoming_direction) / dot(incoming_direction, half_vector))));
        FP_PRECISION specular_term = (d_alpha * g_value * f_theta_value) / (4.0 * dot(incoming_direction, normal) * dot(outgoing_direction, normal));
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