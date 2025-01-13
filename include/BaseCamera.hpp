#pragma once
#include <memory>

#include "../extern/parser.h"
#include "BaseExporter.hpp"
#include "Configuration.hpp"
#include "Helper.hpp"
#include "Ray.hpp"
#include "BaseSpectrum.hpp"

using namespace parser;

class BaseCamera {
 public:
  BaseCamera(
      const Vec3f& position, const Vec3f& gaze, const Vec3f& up,
      const SensorSize sensor_size, const Aperture aperture,
      const ExposureTime exposure_time, const ISO iso, const int pixel_size,
      const int focal_length, const Vec2i sensor_pattern,
      std::vector<std::shared_ptr<BaseSpectrum>> color_filter_array_spectrums,
      std::shared_ptr<BaseSpectrum> quantum_efficiency_spectrum, const float full_well_capacity,
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

  virtual void CalculatePixelValue(const Vec2i& pixel_coordinate);
  virtual void UpdateSampledPixelValue(const Vec2i& pixel_coordinate,
                                       const std::map<int, float>& pixel_value,
                                       const int sample_index,
                                       const Vec2f& diff);

  PixelSample* GetImageSampledDataReference() { return image_sampled_data_; };
  unsigned char* GetImageDataReference() { return image_data_; };
  std::vector<unsigned char>& GetTonemappedImageDataReference() {
    return tonemapped_image_data_;
  };

  virtual void ExportView(const std::shared_ptr<BaseExporter>& exporter) const;

  int image_width_;
  int image_height_;
  unsigned int mem_num_samples_;

 private:
  const std::string image_name_;
  const Vec3f position_;
  const Vec3f u_;
  const Vec3f v_;
  Vec3f q_;
  const Vec2i sensor_pattern_;
  std::vector<std::shared_ptr<BaseSpectrum>> color_filter_array_spectrums_;
  std::shared_ptr<BaseSpectrum> quantum_efficiency_spectrum_;
  const float full_well_capacity_;
  const int quantization_level_;
  
  const ApertureType aperture_type_;

  float sensor_width_;
  float sensor_height_;
  float focal_length_;
  float pixel_size_;
  float exposure_time_;
  float gain_;
  float aperture_size_;

  std::function<std::vector<Vec2f>(int)> pixel_sampling_algorithm_;
  std::function<std::vector<float>(int)> time_sampling_algorithm_;
  std::function<std::vector<Vec2f>(int)> aperture_sampling_algorithm_;

  PixelSample* image_sampled_data_;
  unsigned char* image_data_;
  std::vector<unsigned char> tonemapped_image_data_;

  const float sample_constant_{20.0f};
};