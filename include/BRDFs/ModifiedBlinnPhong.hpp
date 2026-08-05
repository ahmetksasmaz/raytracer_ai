#pragma once

#include "../extern/parser.h"
#include "Spectrum.hpp"
#include "Helper.hpp"
#include "BaseBRDF.hpp"
#include <cmath>
#include <algorithm>

class ModifiedBlinnPhong : public BaseBRDF {
 public:
    ModifiedBlinnPhong(const FP_PRECISION exponent, const bool normalized)
        : BaseBRDF(exponent, normalized) {}
    Spectrum Evaluate(const Vec3f &ray_coming_direction, const Vec3f &light_coming_direction, const Vec3f &normal, const Spectrum &kd, const Spectrum &ks, FP_PRECISION, FP_PRECISION) const override {
        if(!normalized_){
            Vec3f half_vector = normalize(ray_coming_direction + light_coming_direction);
            FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(normal, half_vector));
            FP_PRECISION specular_term = std::pow(NdotH, exponent_);
            return kd + ks * specular_term;
        }
        else{
            Vec3f half_vector = normalize(ray_coming_direction + light_coming_direction);
            FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(normal, half_vector));
            FP_PRECISION specular_term = (exponent_ + 8.0) / (8.0 * M_PI) * std::pow(NdotH, exponent_);
            return kd * (1.0 / M_PI) + ks * specular_term;
        }
    }
    ~ModifiedBlinnPhong() override = default;
};