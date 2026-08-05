#pragma once

#include "../extern/parser.h"
#include "Spectrum.hpp"
#include "Helper.hpp"
#include "BaseBRDF.hpp"
#include <cmath>
#include <algorithm>

class OriginalPhong : public BaseBRDF {
 public:
    OriginalPhong(const FP_PRECISION exponent)
        : BaseBRDF(exponent, false) {}
    Spectrum Evaluate(const Vec3f &ray_coming_direction, const Vec3f &light_coming_direction, const Vec3f &normal, const Spectrum &kd, const Spectrum &ks, FP_PRECISION, FP_PRECISION) const override {
        Vec3f perfect_reflection = normalize(-light_coming_direction + normal * 2.0 * dot(light_coming_direction, normal));
        FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(perfect_reflection, ray_coming_direction));
        FP_PRECISION NdotL = dot(light_coming_direction, normal);
        if (NdotL <= 1e-6) {
            return kd;
        }
        FP_PRECISION specular_term = std::pow(NdotH, exponent_) / NdotL;
        return kd + ks * specular_term;
    }
    ~OriginalPhong() override = default;
};