#include "BaseCamera.hpp"

BaseCamera::BaseCamera(const Vec3f& position, const Vec3f& gaze,
                       const Vec3f& up, const Vec4f& near_plane,
                       const float near_distance, const int image_width,
                       const int image_height, const std::string& image_name)
    : position_(position),
      image_width_(image_width),
      image_height_(image_height),
      image_name_(image_name),
      l_(near_plane.x),
      r_(near_plane.y),
      b_(near_plane.z),
      t_(near_plane.w),
      v_(normalize(up)) {
  image_data_.resize(image_width_ * image_height_ * 3);
  u_ = normalize(cross(normalize(gaze), v_));
  q_ = (v_ * t_) + (position_ + (normalize(gaze) * near_distance) + (u_ * l_));
}

Ray BaseCamera::GenerateRay(const Vec2i& pixel_coordinate) const {
  float su = (pixel_coordinate.x + 0.5) * (r_ - l_) / image_width_;
  float sv = (pixel_coordinate.y + 0.5) * (t_ - b_) / image_height_;

  Vec3f d = normalize((q_ + (u_ * su)) - (v_ * sv) - position_);

  return Ray(position_, d);
}

void BaseCamera::UpdatePixelValue(const Vec2i& pixel_coordinate,
                                  const Vec3uc& pixel_value) {
  int index = (pixel_coordinate.y * image_width_ + pixel_coordinate.x) * 3;
  image_data_[index] = pixel_value.r;
  image_data_[index + 1] = pixel_value.g;
  image_data_[index + 2] = pixel_value.b;
}

void BaseCamera::ExportView(
    const std::shared_ptr<BaseExporter>& exporter) const {
  exporter->Export(image_name_, image_width_, image_height_, image_data_);
}