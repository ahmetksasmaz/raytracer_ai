#pragma once

#include "../extern/parser.h"
#include "Spectrum.hpp"

using namespace parser;

class BaseBRDF {
 public:
    BaseBRDF(const FP_PRECISION exponent, const bool normalized)
        : exponent_(exponent), normalized_(normalized) {}
    virtual Spectrum Evaluate(const Vec3f &ray_coming_direction, const Vec3f &light_coming_direction, const Vec3f &normal, const Spectrum &kd, const Spectrum &ks, FP_PRECISION refractive_index = 1.0, FP_PRECISION absorption_index = 1.0) const = 0;
    virtual ~BaseBRDF() = default;

 protected:
    const FP_PRECISION exponent_;
    const bool normalized_;
};