#pragma once

#include "../extern/parser.h"
#include <algorithm>
#include <cmath>
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
        // Writes one tone-mapped pixel: applies the saturation control, clamps,
        // gamma-encodes and stores. `display_luminance` is whatever the operator
        // computed for this pixel; `linear_rgb` is the original colour, used
        // only to carry hue across.
        //
        // The guard matters. Every operator divides each channel by luminance to
        // recover hue, and for a black pixel that is 0/0. Casting the resulting
        // NaN to unsigned char is undefined behaviour -- on this platform it
        // currently lands on black by luck (arm64 fmax/fmin return the non-NaN
        // operand), but it is a real trap and it moves with the compiler flags.
        void WriteTonemappedPixel(int index, const Vec3f& linear_rgb,
                                  FP_PRECISION display_luminance) {
            const FP_PRECISION luminance = 0.2126 * linear_rgb.x +
                                           0.7152 * linear_rgb.y +
                                           0.0722 * linear_rgb.z;
            if (!(luminance > 1e-10) || !std::isfinite(display_luminance) ||
                !std::isfinite(linear_rgb.x) || !std::isfinite(linear_rgb.y) ||
                !std::isfinite(linear_rgb.z)) {
                tonemapped_image_data_[index * 3 + 0] = 0;
                tonemapped_image_data_[index * 3 + 1] = 0;
                tonemapped_image_data_[index * 3 + 2] = 0;
                return;
            }

            auto encode = [&](FP_PRECISION channel) -> unsigned char {
                const FP_PRECISION saturated =
                    display_luminance * std::pow(channel / luminance, saturation_);
                const FP_PRECISION clamped =
                    std::min(std::max(saturated, static_cast<FP_PRECISION>(0.0)),
                             static_cast<FP_PRECISION>(1.0));
                return static_cast<unsigned char>(255.0 * std::pow(clamped, 1.0 / gamma_));
            };

            tonemapped_image_data_[index * 3 + 0] = encode(linear_rgb.x);
            tonemapped_image_data_[index * 3 + 1] = encode(linear_rgb.y);
            tonemapped_image_data_[index * 3 + 2] = encode(linear_rgb.z);
        }

        const int width_;
        const int height_;
        std::vector<unsigned char> tonemapped_image_data_;
        const FP_PRECISION key_value_;
        const FP_PRECISION burn_;
        const FP_PRECISION saturation_;
        const FP_PRECISION gamma_;
        const std::string filename_;
};