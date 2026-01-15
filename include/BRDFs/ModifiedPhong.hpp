#pragma once

#include "../extern/parser.h"
#include "Helper.hpp"
#include "BaseBRDF.hpp"
#include <cmath>
#include <algorithm>

class ModifiedPhong : public BaseBRDF {
 public:
    ModifiedPhong(const FP_PRECISION exponent, const bool normalized)
        : BaseBRDF(exponent, normalized) {}
    Vec3f Evaluate(const Vec3f &incoming_direction, const Vec3f &outgoing_direction, const Vec3f &normal, const Vec3f &kd, const Vec3f &ks, FP_PRECISION, FP_PRECISION) const override {
        if(!normalized_){
            Vec3f perfect_reflection = normalize(incoming_direction - normal * 2.0 * dot(incoming_direction, normal));
            FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(perfect_reflection, outgoing_direction));
            FP_PRECISION specular_term = std::pow(NdotH, exponent_);
            return kd + ks * specular_term;
        }
        else{
            Vec3f perfect_reflection = normalize(incoming_direction - normal * 2.0 * dot(incoming_direction, normal));
            FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(perfect_reflection, outgoing_direction));
            FP_PRECISION specular_term = (exponent_ + 2.0) / (2.0 * M_PI) * std::pow(NdotH, exponent_);
            return kd * (1.0 / M_PI) + ks * specular_term;
        }
    }
    ~ModifiedPhong() override = default;
};