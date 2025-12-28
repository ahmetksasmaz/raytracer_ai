#include "BaseCamera.hpp"

#include <algorithm>
#include <memory>
#include <string>

BaseCamera::BaseCamera(
    const bool look_at_camera, const Vec3f& position, const Vec3f& gaze,
    const Vec3f& gaze_point, const Vec3f& up, const Vec4f& near_plane,
    const FP_PRECISION fov_y, const FP_PRECISION near_distance, const int image_width,
    const int image_height, const std::string& image_name,
    const unsigned int num_samples, std::vector<std::shared_ptr<BaseToneMapping>> tone_mappings, const SamplingAlgorithm time_sampling,
    const SamplingAlgorithm pixel_sampling, const FP_PRECISION focus_distance,
    const FP_PRECISION aperture_size, const SamplingAlgorithm aperture_sampling,
    const ApertureType aperture_type)
    : position_(position),
      image_width_(image_width),
      image_height_(image_height),
      image_name_(image_name),
      up_(up),
      num_samples_(num_samples),
      tone_mappings_(tone_mappings),
      mem_num_samples_(num_samples ? num_samples : 1),
      focus_distance_(focus_distance),
      aperture_size_(aperture_size),
      aperture_type_(ApertureType::kDefault),
      l_(look_at_camera ? -near_distance * tan(fov_y * M_PI / 360.0f) *
                              FP_PRECISION(image_width) / FP_PRECISION(image_height)
                        : near_plane.x),
      r_(look_at_camera ? near_distance * tan(fov_y * M_PI / 360.0f) *
                              FP_PRECISION(image_width) / FP_PRECISION(image_height)
                        : near_plane.y),
      b_(look_at_camera ? -near_distance * tan(fov_y * M_PI / 360.0f)
                        : near_plane.z),
      t_(look_at_camera ? near_distance * tan(fov_y * M_PI / 360.0f)
                        : near_plane.w),
      u_(look_at_camera ? normalize(cross(gaze_point - position, up))
                        : normalize(cross(gaze, up))),
      v_(look_at_camera ? normalize(cross(u_, gaze_point - position))
                        : normalize(cross(u_, gaze))),
      q_((v_ * t_) +
         (position_ +
          (normalize(look_at_camera ? gaze_point - position : gaze) *
           near_distance) +
          (u_ * l_))),
          ldr_exporter_(std::make_unique<STBExporter>()),
          hdr_exporter_(std::make_unique<EXRExporter>()) {
  image_data_ = new Vec3f[image_width_ * image_height_];
  image_sampled_data_ =
      new Vec5f[image_height_ * image_width_ * mem_num_samples_];
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
  if (!num_samples_) {
    FP_PRECISION su = (pixel_coordinate.x + 0.5) * (r_ - l_) / image_width_;
    FP_PRECISION sv = (pixel_coordinate.y + 0.5) * (t_ - b_) / image_height_;
    Vec3f d = normalize((q_ + (u_ * su)) - (v_ * sv) - position_);

    FP_PRECISION sui = (pixel_coordinate.x + 1 + 0.5) * (r_ - l_) / image_width_;
    FP_PRECISION svj = (pixel_coordinate.y + 1 + 0.5) * (t_ - b_) / image_height_;
    Vec3f di = normalize((q_ + (u_ * sui)) - (v_ * sv) - position_);
    Vec3f dj = normalize((q_ + (u_ * su)) - (v_ * svj) - position_);

    return {Ray(pixel_coordinate, position_, d, {0.5, 0.5}, 0.0, up_, di, dj)};
  }

  std::vector<Ray> rays;

  std::vector<FP_PRECISION> time_samples = time_sampling_algorithm_(num_samples_);

  if (aperture_size_ > 0.0) {
    FP_PRECISION aperture_sample_ratio = 1.0f;

    if (aperture_type_ != ApertureType::kCircular &&
        aperture_type_ != ApertureType::kSquare) {
      FP_PRECISION area_of_unit_circle = M_PI;
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

      FP_PRECISION center_angle_of_primitive_triangle = 2.0 * M_PI / (FP_PRECISION)edge_count;
      FP_PRECISION area_of_primitive_triangle =
          0.5f * sin(center_angle_of_primitive_triangle);
      FP_PRECISION area_of_primitive_polygon = area_of_primitive_triangle * edge_count;
      aperture_sample_ratio = area_of_unit_circle / area_of_primitive_polygon;
    }

    std::vector<Vec2f> aperture_samples = aperture_sampling_algorithm_(
        (int)(num_samples_ * aperture_sample_ratio));
    std::vector<Vec2f> pixel_samples = pixel_sampling_algorithm_(num_samples_);

    if (aperture_type_ != ApertureType::kCircular &&
        aperture_type_ != ApertureType::kSquare) {
      aperture_samples.erase(
          std::remove_if(
              aperture_samples.begin(), aperture_samples.end(),
              [this](const Vec2f& sample) {
                FP_PRECISION radius = sqrt(sample.y);
                FP_PRECISION angle = 2.0f * M_PI * sample.x;

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
                FP_PRECISION sector_angle = 2.0f * M_PI / edge_count;
                FP_PRECISION primitive_triangle_angle = fmod(angle, sector_angle);
                FP_PRECISION primitive_triangle_max_radius_for_angle =
                    1.0f / cos(primitive_triangle_angle);
                return radius > primitive_triangle_max_radius_for_angle;
              }),
          aperture_samples.end());
    }
    while (aperture_samples.size() < num_samples_) {
      if (aperture_type_ == ApertureType::kSquare) {
        aperture_samples.push_back(Vec2f{0.5f, 0.5f});
      } else {
        aperture_samples.push_back(Vec2f{0.0f, 0.0f});
      }
    }

    for (int i = 0; i < num_samples_; i++) {
      auto pixel_sample = pixel_samples[i];
      auto aperture_sample = aperture_samples[i];

      FP_PRECISION su =
          (pixel_coordinate.x + pixel_sample.x) * (r_ - l_) / image_width_;
      FP_PRECISION sv =
          (pixel_coordinate.y + pixel_sample.y) * (t_ - b_) / image_height_;
      Vec3f d = normalize((q_ + (u_ * su)) - (v_ * sv) - position_);

      FP_PRECISION sui =
          (pixel_coordinate.x + 1 + pixel_sample.x) * (r_ - l_) / image_width_;
      FP_PRECISION svj =
          (pixel_coordinate.y + 1 + pixel_sample.y) * (t_ - b_) / image_height_;
      Vec3f di = normalize((q_ + (u_ * sui)) - (v_ * sv) - position_);
      Vec3f dj = normalize((q_ + (u_ * su)) - (v_ * svj) - position_);

      FP_PRECISION t = focus_distance_ / dot(d, normalize(cross(v_, u_)));
      Vec3f focus_point = position_ + (d * t);

      Vec3f aperture_position;

      if (aperture_type_ == ApertureType::kSquare) {
        aperture_position = position_ +
                            (u_ * (aperture_sample.x - 0.5f) * aperture_size_) +
                            (v_ * (aperture_sample.y - 0.5f) * aperture_size_);
      } else {
        FP_PRECISION r = aperture_size_ / 2.0f;
        FP_PRECISION theta = 2.0f * M_PI * aperture_sample.x;
        FP_PRECISION s = r * sqrt(aperture_sample.y);
        FP_PRECISION x = s * cos(theta);
        FP_PRECISION y = s * sin(theta);
        aperture_position = position_ + (u_ * x) + (v_ * y);
      }


      Ray ray(pixel_coordinate, aperture_position,
              normalize(focus_point - aperture_position),
              {pixel_sample.x, pixel_sample.y}, time_samples[i], up_, di, dj);

      rays.push_back(ray);
    }
  } else {
    std::vector<Vec2f> samples = pixel_sampling_algorithm_(num_samples_);
    for (int i = 0; i < samples.size(); i++) {
      FP_PRECISION su = (pixel_coordinate.x + samples[i].x) * (r_ - l_) / image_width_;
      FP_PRECISION sv =
          (pixel_coordinate.y + samples[i].y) * (t_ - b_) / image_height_;
      Vec3f d = normalize((q_ + (u_ * su)) - (v_ * sv) - position_);

      FP_PRECISION sui =
          (pixel_coordinate.x + 1 + samples[i].x) * (r_ - l_) / image_width_;
      FP_PRECISION svj =
          (pixel_coordinate.y + 1 + samples[i].y) * (t_ - b_) / image_height_;
      Vec3f di = normalize((q_ + (u_ * sui)) - (v_ * sv) - position_);
      Vec3f dj = normalize((q_ + (u_ * su)) - (v_ * svj) - position_);

      rays.push_back(Ray(pixel_coordinate, position_, d,
                         {samples[i].x, samples[i].y}, time_samples[i], up_, di, dj));
    }
  }
  return rays;
}

void BaseCamera::UpdateSampledPixelValue(const Vec2i& pixel_coordinate,
                                         const Vec3f& pixel_value,
                                         const int sample_index,
                                         const Vec2f& diff) {
  image_sampled_data_[(pixel_coordinate.y * image_width_ + pixel_coordinate.x) *
                          mem_num_samples_ +
                      sample_index] =
      Vec5f{pixel_value.x, pixel_value.y, pixel_value.z, diff.x, diff.y};
}

void BaseCamera::UpdatePixelValue(const Vec2i& pixel_coordinate,
                                  const Vec3f& pixel_value) {
  int index = (pixel_coordinate.y * image_width_ + pixel_coordinate.x);
  image_data_[index] = pixel_value;
}

void BaseCamera::ApplyToneMappings() {
  for(auto& tonemapping : tone_mappings_){
    tonemapping->ApplyToneMapping(image_data_);
  }
}

void BaseCamera::ExportView() {
  std::string extension = image_name_.substr(image_name_.find_last_of(".") + 1);
  std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

  if(extension == "exr"){
    float * image_data_float = new float[image_width_ * image_height_ * 3];

    for(int i = 0; i < image_width_ * image_height_; i++){
      image_data_float[i * 3 + 0] = static_cast<float>(image_data_[i].x);
      image_data_float[i * 3 + 1] = static_cast<float>(image_data_[i].y);
      image_data_float[i * 3 + 2] = static_cast<float>(image_data_[i].z);
    }

    hdr_exporter_->Export(
        image_name_, image_width_, image_height_, image_data_float);
    
    delete[] image_data_float;

    for (auto& tonemapping : tone_mappings_){
      ldr_exporter_->Export(
        tonemapping->GetFilename(), image_width_, image_height_, tonemapping->GetTonemappedImageDataReference().data());
    }
  }
  else{
    unsigned char * image_data_uchar = new unsigned char[image_width_ * image_height_ * 3];

    for(int i = 0; i < image_width_ * image_height_; i++){
      image_data_uchar[i * 3 + 0] = static_cast<unsigned char>(std::max(0.0, std::min(255.0, image_data_[i].x)));
      image_data_uchar[i * 3 + 1] = static_cast<unsigned char>(std::max(0.0, std::min(255.0, image_data_[i].y)));
      image_data_uchar[i * 3 + 2] = static_cast<unsigned char>(std::max(0.0, std::min(255.0, image_data_[i].z)));
    }

    ldr_exporter_->Export(
        image_name_, image_width_, image_height_, image_data_uchar);
    
    delete[] image_data_uchar;
  }
}