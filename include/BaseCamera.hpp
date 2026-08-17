#pragma once
#include <atomic>
#include <functional>
#include <memory>

#include "../extern/parser.h"
#include "STBExporter.hpp"
#include "EXRExporter.hpp"
#include "Configuration.hpp"
#include "Spectrum.hpp"
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
    delete[] accumulator_;
    delete[] reflectance_accumulator_;
    delete[] illumination_accumulator_;
    delete[] spectral_image_;
    delete[] reflectance_image_;
    delete[] illumination_image_;
    delete[] image_data_;
  };

  virtual std::vector<Ray> GenerateRay(const Vec2i& pixel_coordinate) const;

  virtual void UpdatePixelValue(const Vec2i& pixel_coordinate,
                                const Vec3f& pixel_value);

  // Adds one sample to the film.
  //
  // The sample is splatted into the 3x3 neighbourhood with its reconstruction
  // weight rather than stored. The previous design kept every individual sample
  // (width x height x samples-per-pixel), which is unusable once radiance is
  // spectral: 800x800 at 1000 spp and 31 bands would be nearly 200 GB. Film
  // memory is now independent of sample count.
  //
  // Splats cross the 32x32 tile boundaries each thread owns, so accumulation is
  // atomic.
  virtual void SplatSample(const Vec2i& pixel_coordinate,
                           const Spectrum& pixel_value,
                           const Spectrum& reflectance,
                           const Spectrum& illumination, const Vec2f& diff);

  // Whether this camera is collecting the reflectance/illumination AOVs. The
  // schedulers ask before tracing: when false the tracer is handed nullptr and
  // skips the extra BRDF evaluations entirely, so --no-aov costs nothing.
  bool CollectsAOVs() const { return reflectance_accumulator_ != nullptr; }

  // Allocates the AOV buffers. Called once, before rendering starts; the
  // buffers are two thirds of the film's memory, so they are not allocated
  // unless someone asked for them.
  void EnableAOVs();

  // Divides the accumulated film by its weights, producing both the spectral
  // image and the conventional linear-sRGB image. Call once per camera after
  // tracing completes.
  void ResolveAccumulator();

  // Writes <base>_radiance.exr, the spectral radiance cube that the sensor and
  // ISP programs consume. Written for every render.
  void ExportSpectralRadiance();

  Vec3f* GetImageDataReference() { return image_data_; };
  const Spectrum* GetSpectralImage() const { return spectral_image_; };

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

  // width * height * (kSpectralBands + 1): the per-pixel band sums followed by
  // the summed reconstruction weight.
  std::atomic<FP_PRECISION>* accumulator_;
  static constexpr int kAccumStride = kSpectralBands + 1;

  // The two AOV films, width * height * kSpectralBands each. They deliberately
  // do NOT carry their own weight channel: all three buffers are written by the
  // same sample with the same reconstruction weight, so a second copy could
  // only ever drift out of step with the first.
  //
  // nullptr when AOVs are off, which is also how CollectsAOVs() answers.
  std::atomic<FP_PRECISION>* reflectance_accumulator_ = nullptr;
  std::atomic<FP_PRECISION>* illumination_accumulator_ = nullptr;

  Spectrum* spectral_image_;  // resolved, sensor-independent
  Spectrum* reflectance_image_ = nullptr;
  Spectrum* illumination_image_ = nullptr;
  Vec3f* image_data_;         // resolved, linear sRGB, conventional output
};