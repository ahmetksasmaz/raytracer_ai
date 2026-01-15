#pragma once

#include "../extern/parser.h"
#include "Helper.hpp"
#include "BaseBRDF.hpp"
#include <cmath>
#include <algorithm>

class OriginalBlinnPhong : public BaseBRDF {
 public:
    OriginalBlinnPhong(const FP_PRECISION exponent)
        : BaseBRDF(exponent, false) {}
    Vec3f Evaluate(const Vec3f &incoming_direction, const Vec3f &outgoing_direction, const Vec3f &normal, const Vec3f &kd, const Vec3f &ks, FP_PRECISION) const override {
        Vec3f half_vector = normalize(incoming_direction + outgoing_direction);
        FP_PRECISION NdotH = std::max((FP_PRECISION)0.0, dot(normal, half_vector));
        FP_PRECISION specular_term = std::pow(NdotH, exponent_);
        specular_term /= dot(incoming_direction, normal);
        return kd + ks * specular_term;
    }
    ~OriginalBlinnPhong() override = default;
};