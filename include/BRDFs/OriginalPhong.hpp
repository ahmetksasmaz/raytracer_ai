#pragma once

#include "../extern/parser.h"
#include "Helper.hpp"
#include "BaseBRDF.hpp"
#include <cmath>
#include <algorithm>

class OriginalPhong : public BaseBRDF {
 public:
    OriginalPhong(const FP_PRECISION exponent)
        : BaseBRDF(exponent, false) {}
    Vec3f Evaluate(const Vec3f &incoming_direction, const Vec3f &outgoing_direction, const Vec3f &normal, const Vec3f &kd, const Vec3f &ks, FP_PRECISION, FP_PRECISION) const override {
        Vec3f perfect_reflection = normalize(incoming_direction - normal * 2.0 * dot(incoming_direction, normal));
        FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(perfect_reflection, outgoing_direction));
        FP_PRECISION specular_term = std::pow(NdotH, exponent_);
        specular_term /= dot(incoming_direction, normal);
        return kd + ks * specular_term;
    }
    ~OriginalPhong() override = default;
};