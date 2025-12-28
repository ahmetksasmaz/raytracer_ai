#pragma once

#include "../extern/parser.h"
#include <vector>

using namespace parser;

class BaseToneMapping {
    public:
        BaseToneMapping(int width, int height, FP_PRECISION key_value = 0.18, FP_PRECISION burn = 0.0, FP_PRECISION saturation = 1.0, FP_PRECISION gamma = 1.0, std::string filename = ".png")
            : width_(width), height_(height), key_value_(key_value), burn_(burn), saturation_(saturation), gamma_(gamma), filename_(filename) {
                tonemapped_image_data_.resize(width * height * 3);
            }
        virtual ~BaseToneMapping() = default;
        virtual void ApplyToneMapping(Vec3f* image_data) = 0;
        std::string GetFilename() const { return filename_; }
        int GetWidth() const { return width_; }
        int GetHeight() const { return height_; }
        std::vector<unsigned char>& GetTonemappedImageDataReference() {
            return tonemapped_image_data_;
        }
    protected:
        const int width_;
        const int height_;
        std::vector<unsigned char> tonemapped_image_data_;
        const FP_PRECISION key_value_;
        const FP_PRECISION burn_;
        const FP_PRECISION saturation_;
        const FP_PRECISION gamma_;
        const std::string filename_;
};