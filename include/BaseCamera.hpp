#pragma once
#include <memory>

#include "../extern/parser.h"
#include "BaseExporter.hpp"
#include "Configuration.hpp"
#include "Helper.hpp"
#include "Ray.hpp"

using namespace parser;

class BaseCamera {
 public:
  BaseCamera(
      const Vec3f& position, const Vec3f& gaze, const Vec3f& up,
      const Vec4f& near_plane, const float near_distance, const int image_width,
      const int image_height, const std::string& image_name,
      const unsigned int num_samples = 0,
      const SamplingAlgorithm time_sampling = SamplingAlgorithm::kBest,
      const SamplingAlgorithm pixel_sampling = SamplingAlgorithm::kBest,
      const float focus_distance = 0.0, const float aperture_size = 0.0,
      const SamplingAlgorithm aperture_sampling = SamplingAlgorithm::kBest,
      const ApertureType aperture_type = ApertureType::kDefault);
  virtual ~BaseCamera() = default;

  virtual std::vector<Ray> GenerateRay(const Vec2i& pixel_coordinate) const;

  virtual void UpdatePixelValue(const Vec2i& pixel_coordinate,
                                const Vec3f& pixel_value);
  virtual void UpdateSampledPixelValue(const Vec2i& pixel_coordinate,
                                       const Vec3f& pixel_value,
                                       const Vec2f& diff);

  std::vector<std::vector<std::vector<std::pair<Vec3f, Vec2f>>>>&
  GetImageSampledDataReference() {
    return image_sampled_data_;
  };
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
  const Vec3f u_;
  const Vec3f v_;
  const Vec3f q_;

  const unsigned int num_samples_;
  const float focus_distance_;
  const float aperture_size_;

  const ApertureType aperture_type_;

  std::function<std::vector<Vec2f>(int)> pixel_sampling_algorithm_;
  std::function<std::vector<float>(int)> time_sampling_algorithm_;
  std::function<std::vector<Vec2f>(int)> aperture_sampling_algorithm_;

  std::vector<std::vector<std::vector<std::pair<Vec3f, Vec2f>>>>
      image_sampled_data_;
  std::vector<Vec3f> image_data_;
  std::vector<unsigned char> tonemapped_image_data_;
};