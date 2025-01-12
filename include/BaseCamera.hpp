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
      const SensorSize sensor_size, const Aperture aperture,
      const ExposureTime exposure_time, const ISO iso, const int pixel_size,
      const int focal_length, const Vec2i sensor_pattern,
      const std::vector<int> color_filter_array_spectrums,
      const int quantum_efficiency_spectrum, const float full_well_capacity,
      const int quantization_level, const std::string& image_name,
      const SamplingAlgorithm time_sampling = SamplingAlgorithm::kBest,
      const SamplingAlgorithm pixel_sampling = SamplingAlgorithm::kBest,
      const SamplingAlgorithm aperture_sampling = SamplingAlgorithm::kBest,
      const ApertureType aperture_type = ApertureType::kDefault);
  virtual ~BaseCamera() {
    delete[] image_sampled_data_;
    delete[] image_data_;
  };

  virtual std::vector<Ray> GenerateRay(const Vec2i& pixel_coordinate) const;

  virtual void UpdatePixelValue(const Vec2i& pixel_coordinate,
                                const Vec3f& pixel_value);
  virtual void UpdateSampledPixelValue(const Vec2i& pixel_coordinate,
                                       const Vec3f& pixel_value,
                                       const int sample_index,
                                       const Vec2f& diff);

  Vec5f* GetImageSampledDataReference() { return image_sampled_data_; };
  Vec3f* GetImageDataReference() { return image_data_; };
  std::vector<unsigned char>& GetTonemappedImageDataReference() {
    return tonemapped_image_data_;
  };

  virtual void ExportView(const std::shared_ptr<BaseExporter>& exporter) const;

  const int image_width_;
  const int image_height_;
  const unsigned int mem_num_samples_;

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

  Vec5f* image_sampled_data_;
  Vec3f* image_data_;
  std::vector<unsigned char> tonemapped_image_data_;
};