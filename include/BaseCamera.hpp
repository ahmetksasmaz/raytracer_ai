#pragma once
#include <memory>

#include "../extern/parser.h"
#include "BaseExporter.hpp"
#include "CameraRay.hpp"
#include "Pixel.hpp"

using namespace parser;

class BaseCamera {
 public:
  BaseCamera(Vec3f position, Vec3f gaze, Vec3f up, float near_plane,
             float near_distance, float far_distance, int image_width,
             int image_height, std::string image_name);
  virtual ~BaseCamera();

  virtual CameraRay GenerateRay(Vec2i pixel_coordinate) = 0;

  virtual void ExportView(std::shared_ptr<BaseExporter> exporter) = 0;

 private:
  Vec3f position_;
  Vec3f gaze_;
  Vec3f up_;
  float near_plane_;
  float near_distance_;
  float far_distance_;
  int image_width_;
  int image_height_;
  std::string image_name_;
  std::vector<std::shared_ptr<Pixel>> image_data_;
};