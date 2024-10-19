#pragma once
#include <memory>

#include "../extern/parser.h"
#include "BaseExporter.hpp"
#include "Helper.hpp"
#include "Ray.hpp"

using namespace parser;

class BaseCamera {
 public:
  BaseCamera(const Vec3f& position, const Vec3f& gaze, const Vec3f& up,
             const Vec4f& near_plane, const float near_distance,
             const int image_width, const int image_height,
             const std::string& image_name);
  virtual ~BaseCamera() = default;

  virtual Ray GenerateRay(const Vec2i& pixel_coordinate) const;

  virtual void UpdatePixelValue(const Vec2i& pixel_coordinate,
                                const Vec3f& pixel_value);

  std::vector<Vec3f>& GetImageDataReference() { return image_data_; };
  std::vector<unsigned char>& GetTonemappedImageDataReference() {
    return tonemapped_image_data_;
  };

  virtual void ExportView(const std::shared_ptr<BaseExporter>& exporter) const;

  const int image_width_;
  const int image_height_;

 private:
  const std::string image_name_;
  const Vec3f position_;
  const float l_;
  const float r_;
  const float b_;
  const float t_;
  const Vec3f v_;
  const Vec3f u_;
  const Vec3f q_;

  std::vector<Vec3f> image_data_;
  std::vector<unsigned char> tonemapped_image_data_;
};