#pragma once

#include "BaseToneMapping.hpp"

class FilmicToneMapping : public BaseToneMapping {
    public:
        FilmicToneMapping(int width, int height, FP_PRECISION key_value = 0.18, FP_PRECISION burn = 0.0, FP_PRECISION saturation = 1.0, FP_PRECISION gamma = 1.0, std::string extension = "_film.png")
            : BaseToneMapping(width, height, key_value, burn, saturation, gamma, extension) {}
        virtual ~FilmicToneMapping() = default;
        virtual void ApplyToneMapping(Vec3f* image_data) override;
};