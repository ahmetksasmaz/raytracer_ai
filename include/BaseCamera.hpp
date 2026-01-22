#pragma once
#include <memory>

#include "../extern/parser.h"
#include "STBExporter.hpp"
#include "EXRExporter.hpp"
#include "Configuration.hpp"
#include "Helper.hpp"
#include "Ray.hpp"
#include "BaseToneMapping.hpp"

using namespace parser;

class BaseCamera {
 public:
  BaseCamera(
      const bool look_at_camera, const Vec3f& position, const Vec3f& gaze,
      const Vec3f& gaze_point, const Vec3f& up, const Vec4f& near_plane,
      const FP_PRECISION fov_y, const FP_PRECISION near_distance, const int image_width,
      const int image_height, const std::string& image_name,
      const unsigned int num_samples = 0,
      std::vector<std::shared_ptr<BaseToneMapping>> tone_mappings = {},
      int max_recursion_depth = 1,
      int min_recursion_depth = 0,
      bool left_handed = false,
      bool path_tracing_enabled = false,
      bool importance_sampling_enabled = false,
      bool nee_enabled = false,
      bool mis_balance_enabled = false,
      bool russian_roulette_enabled = false,
      int splitting_factor = 1,
      FP_PRECISION sample_max_val = 0.0f,
      const SamplingAlgorithm time_sampling = SamplingAlgorithm::kBest,
      const SamplingAlgorithm pixel_sampling = SamplingAlgorithm::kBest,
      const FP_PRECISION focus_distance = 0.0, const FP_PRECISION aperture_size = 0.0,
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

  void ApplyToneMappings();
  void ExportView();

  const int image_width_;
  const int image_height_;
  const unsigned int mem_num_samples_;
  const int max_recursion_depth_;
  const int min_recursion_depth_;
  const bool path_tracing_enabled_;
  const bool importance_sampling_enabled_;
  const bool nee_enabled_;
  const bool mis_balance_enabled_;
  const bool russian_roulette_enabled_;
  const int splitting_factor_;
  const FP_PRECISION sample_max_val_;

 private:
  const std::string image_name_;
  const Vec3f position_;
  const FP_PRECISION l_;
  const FP_PRECISION r_;
  const FP_PRECISION b_;
  const FP_PRECISION t_;
  const Vec3f u_;
  const Vec3f v_;
  const Vec3f q_;
  const Vec3f up_;

  const unsigned int num_samples_;
  const FP_PRECISION focus_distance_;
  const FP_PRECISION aperture_size_;
  const bool left_handed_;

  const ApertureType aperture_type_;
  const std::vector<std::shared_ptr<BaseToneMapping>> tone_mappings_;

  std::function<std::vector<Vec2f>(int)> pixel_sampling_algorithm_;
  std::function<std::vector<FP_PRECISION>(int)> time_sampling_algorithm_;
  std::function<std::vector<Vec2f>(int)> aperture_sampling_algorithm_;

  const std::unique_ptr<STBExporter> ldr_exporter_;
  const std::unique_ptr<EXRExporter> hdr_exporter_;

  Vec5f* image_sampled_data_;
  Vec3f* image_data_;
};