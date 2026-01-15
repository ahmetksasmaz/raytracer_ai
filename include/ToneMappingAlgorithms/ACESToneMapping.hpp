#pragma once

#include "BaseToneMapping.hpp"

class ACESToneMapping : public BaseToneMapping {
    public:
        ACESToneMapping(int width, int height, FP_PRECISION key_value = 0.18, FP_PRECISION burn = 0.0, FP_PRECISION saturation = 1.0, FP_PRECISION gamma = 1.0, std::string extension = "_aces.png")
            : BaseToneMapping(width, height, key_value, burn, saturation, gamma, extension) {}
        virtual ~ACESToneMapping() = default;
        virtual void ApplyToneMapping(Vec3f* image_data) override;
};