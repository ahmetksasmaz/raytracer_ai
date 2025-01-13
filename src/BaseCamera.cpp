#include "BaseCamera.hpp"

BaseCamera::BaseCamera(
      const Vec3f& position, const Vec3f& gaze, const Vec3f& up,
      const SensorSize sensor_size, const Aperture aperture,
      const ExposureTime exposure, const ISO iso, const int pixel_size,
      const int focal_length, const Vec2i sensor_pattern,
      std::vector<std::shared_ptr<BaseSpectrum>> color_filter_array_spectrums,
      std::shared_ptr<BaseSpectrum> quantum_efficiency_spectrum,
      const float full_well_capacity,
      const int quantization_level, const std::string& image_name,
      const SamplingAlgorithm time_sampling,
      const SamplingAlgorithm pixel_sampling,
      const SamplingAlgorithm aperture_sampling,
      const ApertureType aperture_type)
    : position_(position),
      sensor_pattern_(sensor_pattern),
      color_filter_array_spectrums_(color_filter_array_spectrums),
      quantum_efficiency_spectrum_(quantum_efficiency_spectrum),
      full_well_capacity_(full_well_capacity),
      quantization_level_(quantization_level),
      image_name_(image_name),
      aperture_type_(ApertureType::kDefault),
      u_(normalize(cross(gaze, up))),
      v_(normalize(cross(u_, gaze))){
  switch (sensor_size) {
    case SensorSize::kFullFrame:
      sensor_width_ = 36.0f;
      sensor_height_ = 24.0f;
      break;
    case SensorSize::kAPSC:
      sensor_width_ = 22.2f;
      sensor_height_ = 14.8f;
      break;
    case SensorSize::kFourThirds:
      sensor_width_ = 17.3f;
      sensor_height_ = 13.0f;
      break;
    case SensorSize::kOneInch:
      sensor_width_ = 13.2f;
      sensor_height_ = 8.8f;
      break;
    case SensorSize::kTwoOverThree:
      sensor_width_ = 23.6f;
      sensor_height_ = 15.7f;
      break;
    default:
      sensor_width_ = 36.0f;
      sensor_height_ = 24.0f;
      break;
    }
      sensor_width_ = sensor_width_ / 1000.0f; // mm to m
      sensor_height_ = sensor_height_ / 1000.0f; // mm to m
      pixel_size_ = float(pixel_size) / 1000000.0f; // um to m

      image_width_ = sensor_width_ / pixel_size_;
      image_height_ = sensor_height_ / pixel_size_;

      focal_length_ = focal_length / 1000.0f; // mm to m

      q_ = (v_ * sensor_height_ / 2) +
        (position_ +
        (normalize(gaze) *
          focal_length_) +
        (u_ * -sensor_width_ / 2));
    
    float aperture_multiplier;

    switch(aperture){
      case Aperture::kF1_0:
        aperture_size_ = focal_length_ / 1.0;
        aperture_multiplier = 1.0 * 1.0;
        break;
      case Aperture::kF1_4:
        aperture_size_ = focal_length_ / 1.4;
        aperture_multiplier = 1.4 * 1.4;
        break;
      case Aperture::kF2_0:
        aperture_size_ = focal_length_ / 2.0;
        aperture_multiplier = 2.0 * 2.0;
        break;
      case Aperture::kF2_8:
        aperture_size_ = focal_length_ / 2.8;
        aperture_multiplier = 2.8 * 2.8;
        break;
      case Aperture::kF4_0:
        aperture_size_ = focal_length_ / 4.0;
        aperture_multiplier = 4.0 * 4.0;
        break;
      case Aperture::kF5_6:
        aperture_size_ = focal_length_ / 5.6;
        aperture_multiplier = 5.6 * 5.6;
        break;
      case Aperture::kF8_0:
        aperture_size_ = focal_length_ / 8.0;
        aperture_multiplier = 8.0 * 8.0;
        break;
      case Aperture::kF11_0:
        aperture_size_ = focal_length_ / 11.0;
        aperture_multiplier = 11.0 * 11.0;
        break;
      case Aperture::kF16_0:
        aperture_size_ = focal_length_ / 16.0;
        aperture_multiplier = 16.0 * 16.0;
        break;
      case Aperture::kF22_0:
        aperture_size_ = focal_length_ / 22.0;
        aperture_multiplier = 22.0 * 22.0;
        break;
      default:
        aperture_size_ = focal_length_ / 1.0;
        aperture_multiplier = 1.0 * 1.0;
        break;
    }

    switch (exposure) {
      case ExposureTime::k1_8000:
        exposure_time_ = 1.0f / 8000.0f;
        break;
      case ExposureTime::k1_4000:
        exposure_time_ = 1.0f / 4000.0f;
        break;
      case ExposureTime::k1_2000:
        exposure_time_ = 1.0f / 2000.0f;
        break;
      case ExposureTime::k1_1000:
        exposure_time_ = 1.0f / 1000.0f;
        break;
      case ExposureTime::k1_500:
        exposure_time_ = 1.0f / 500.0f;
        break;
      case ExposureTime::k1_250:
        exposure_time_ = 1.0f / 250.0f;
        break;
      case ExposureTime::k1_125:
        exposure_time_ = 1.0f / 125.0f;
        break;
      case ExposureTime::k1_60:
        exposure_time_ = 1.0f / 60.0f;
        break;
      case ExposureTime::k1_30:
        exposure_time_ = 1.0f / 30.0f;
        break;
      case ExposureTime::k1_15:
        exposure_time_ = 1.0f / 15.0f;
        break;
      case ExposureTime::k1_8:
        exposure_time_ = 1.0f / 8.0f;
        break;
      case ExposureTime::k1_4:
        exposure_time_ = 1.0f / 4.0f;
        break;
      case ExposureTime::k1_2:
        exposure_time_ = 1.0f / 2.0f;
        break;
      case ExposureTime::k1:
        exposure_time_ = 1.0f;
        break;
      case ExposureTime::k2:
        exposure_time_ = 2.0f;
        break;
      case ExposureTime::k4:
        exposure_time_ = 4.0f;
        break;
      case ExposureTime::k8:
        exposure_time_ = 8.0f;
        break;
      case ExposureTime::k15:
        exposure_time_ = 15.0f;
        break;
      case ExposureTime::k30:
        exposure_time_ = 30.0f;
        break;
      default:
        exposure_time_ = 1.0f;
        break;
    }

    switch(iso){
      case ISO::k100:
        gain_ = 1.0f;
        break;
      case ISO::k200:
        gain_ = 2.0f;
        break;
      case ISO::k400:
        gain_ = 4.0f;
        break;
      case ISO::k800:
        gain_ = 8.0f;
        break;
      case ISO::k1600:
        gain_ = 16.0f;
        break;
      case ISO::k3200:
        gain_ = 32.0f;
        break;
      case ISO::k6400:
        gain_ = 64.0f;
        break;
      case ISO::k12800:
        gain_ = 128.0f;
        break;
      case ISO::k25600:
        gain_ = 256.0f;
        break;
      case ISO::k51200:
        gain_ = 512.0f;
        break;
      case ISO::k102400:
        gain_ = 1024.0f;
        break;
      default:
        gain_ = 1.0f;
        break;
    }

  mem_num_samples_ = sample_constant_ * (float(pixel_size) * float(pixel_size)) / aperture_multiplier;

  image_data_ = new unsigned char[image_width_ * image_height_];
  image_sampled_data_ =
      new PixelSample[image_height_ * image_width_ * mem_num_samples_];
  tonemapped_image_data_.resize(image_width_ * image_height_ * 3);

  switch (time_sampling) {
    case SamplingAlgorithm::kUniform:
      time_sampling_algorithm_ = uniform_1d;
      break;
    case SamplingAlgorithm::kRandom:
      time_sampling_algorithm_ = uniform_random_1d;
      break;
    case SamplingAlgorithm::kJittered:
      time_sampling_algorithm_ = jittered_1d;
      break;
  }

  switch (pixel_sampling) {
    case SamplingAlgorithm::kUniform:
      pixel_sampling_algorithm_ = uniform_2d;
      break;
    case SamplingAlgorithm::kRandom:
      pixel_sampling_algorithm_ = uniform_random_2d;
      break;
    case SamplingAlgorithm::kJittered:
      pixel_sampling_algorithm_ = jittered_2d;
      break;
    case SamplingAlgorithm::kMultiJittered:
      pixel_sampling_algorithm_ = multi_jittered_2d;
      break;
    case SamplingAlgorithm::kHalton:
      pixel_sampling_algorithm_ = halton_2d;
      break;
    case SamplingAlgorithm::kHammersley:
      pixel_sampling_algorithm_ = hammersley_2d;
      break;
  }

  switch (aperture_sampling) {
    case SamplingAlgorithm::kUniform:
      aperture_sampling_algorithm_ = uniform_2d;
      break;
    case SamplingAlgorithm::kRandom:
      aperture_sampling_algorithm_ = uniform_random_2d;
      break;
    case SamplingAlgorithm::kJittered:
      aperture_sampling_algorithm_ = jittered_2d;
      break;
    case SamplingAlgorithm::kMultiJittered:
      aperture_sampling_algorithm_ = multi_jittered_2d;
      break;
    case SamplingAlgorithm::kHalton:
      aperture_sampling_algorithm_ = halton_2d;
      break;
    case SamplingAlgorithm::kHammersley:
      aperture_sampling_algorithm_ = hammersley_2d;
      break;
  }
}

std::vector<Ray> BaseCamera::GenerateRay(const Vec2i& pixel_coordinate) const {

  int cfa_index = (pixel_coordinate.y % sensor_pattern_.y) * sensor_pattern_.x +
                  (pixel_coordinate.x % sensor_pattern_.x);

  auto cfa_spectrum = color_filter_array_spectrums_[cfa_index];

  std::vector<Ray> rays;


  std::vector<float> time_samples = time_sampling_algorithm_(mem_num_samples_);

  float aperture_sample_ratio = 1.0f;

  if (aperture_type_ != ApertureType::kCircular &&
      aperture_type_ != ApertureType::kSquare) {
    float area_of_unit_circle = M_PI;
    int edge_count;

    switch (aperture_type_) {
      case ApertureType::kPoly3:
        edge_count = 3;
        break;
      case ApertureType::kPoly5:
        edge_count = 5;
        break;
      case ApertureType::kPoly6:
        edge_count = 6;
        break;
    }

    float center_angle_of_primitive_triangle = 2.0 * M_PI / (float)edge_count;
    float area_of_primitive_triangle =
        0.5f * sin(center_angle_of_primitive_triangle);
    float area_of_primitive_polygon = area_of_primitive_triangle * edge_count;
    aperture_sample_ratio = area_of_unit_circle / area_of_primitive_polygon;
  }

  std::vector<Vec2f> aperture_samples = aperture_sampling_algorithm_(
      (int)(mem_num_samples_ * aperture_sample_ratio));
  std::vector<Vec2f> pixel_samples = pixel_sampling_algorithm_(mem_num_samples_);

  if (aperture_type_ != ApertureType::kCircular &&
      aperture_type_ != ApertureType::kSquare) {
    aperture_samples.erase(
        std::remove_if(
            aperture_samples.begin(), aperture_samples.end(),
            [this](const Vec2f& sample) {
              float radius = sqrt(sample.y);
              float angle = 2.0f * M_PI * sample.x;

              int edge_count;
              switch (aperture_type_) {
                case ApertureType::kPoly3:
                  edge_count = 3;
                  break;
                case ApertureType::kPoly5:
                  edge_count = 5;
                  break;
                case ApertureType::kPoly6:
                  edge_count = 6;
                  break;
                default:
                  return false;
              }
              float sector_angle = 2.0f * M_PI / edge_count;
              float primitive_triangle_angle = fmod(angle, sector_angle);
              float primitive_triangle_max_radius_for_angle =
                  1.0f / cos(primitive_triangle_angle);
              return radius > primitive_triangle_max_radius_for_angle;
            }),
        aperture_samples.end());
  }
  while (aperture_samples.size() < mem_num_samples_) {
    if (aperture_type_ == ApertureType::kSquare) {
      aperture_samples.push_back(Vec2f{0.5f, 0.5f});
    } else {
      aperture_samples.push_back(Vec2f{0.0f, 0.0f});
    }
  }

  for (int i = 0; i < mem_num_samples_; i++) {
    auto pixel_sample = pixel_samples[i];
    auto aperture_sample = aperture_samples[i];

    float su =
        (pixel_coordinate.x + pixel_sample.x) * (pixel_size_);
    float sv =
        (pixel_coordinate.y + pixel_sample.y) * (pixel_size_);
    Vec3f d = normalize((q_ + (u_ * su)) - (v_ * sv) - position_);
    float t = (focal_length_ * 2.0) / dot(d, normalize(cross(v_, u_)));
    Vec3f focus_point = position_ + (d * t);

    Vec3f aperture_position;

    if (aperture_type_ == ApertureType::kSquare) {
      aperture_position = position_ +
                          (u_ * (aperture_sample.x - 0.5f) * aperture_size_) +
                          (v_ * (aperture_sample.y - 0.5f) * aperture_size_);
    } else {
      float r = aperture_size_ / 2.0f;
      float theta = 2.0f * M_PI * aperture_sample.x;
      float s = r * sqrt(aperture_sample.y);
      float x = s * cos(theta);
      float y = s * sin(theta);
      aperture_position = position_ + (u_ * x) + (v_ * y);
    }
    

    Ray ray(cfa_spectrum->GetSamples(), pixel_coordinate, aperture_position,
            normalize(focus_point - aperture_position),
            {pixel_sample.x, pixel_sample.y}, time_samples[i]);

    rays.push_back(ray);
  }
  return rays;
}

void BaseCamera::UpdateSampledPixelValue(const Vec2i& pixel_coordinate,
                                         const std::map<int, float>& pixel_value,
                                         const int sample_index,
                                         const Vec2f& diff) {
  image_sampled_data_[(pixel_coordinate.y * image_width_ + pixel_coordinate.x) *
                          mem_num_samples_ +
                      sample_index] =
      PixelSample{pixel_value, diff.x, diff.y};
}

void BaseCamera::CalculatePixelValue(const Vec2i& pixel_coordinate) {

  int cfa_index = (pixel_coordinate.y % sensor_pattern_.y) * sensor_pattern_.x +
                  (pixel_coordinate.x % sensor_pattern_.x);

  auto cfa_spectrum = color_filter_array_spectrums_[cfa_index];

  float final_sum = 0.0f;

  for(int sample_index = 0; sample_index < mem_num_samples_; sample_index++){
    auto &values = image_sampled_data_[(pixel_coordinate.y * image_width_ + pixel_coordinate.x) *
                          mem_num_samples_ +
                      sample_index].values_;
    for(auto &pair : values){
      final_sum += (*cfa_spectrum)[pair.first] * pair.second;
    }
  }
  

  int index = (pixel_coordinate.y * image_width_ + pixel_coordinate.x);
  image_data_[index] = (unsigned char)(final_sum * 255.0 / full_well_capacity_);
  // WILL BE UPDATED
}

void BaseCamera::ExportView(
    const std::shared_ptr<BaseExporter>& exporter) const {
  // exporter->Export(image_name_, tonemapped_image_data_.data(), image_width_,
  //                  image_height_);
  exporter->Export(image_name_, image_data_, image_width_,
                   image_height_);
                  
}