#pragma once
#include "../extern/parser.h"

using namespace parser;

class BaseImage {
 public:
  BaseImage(const std::string& path);
  virtual ~BaseImage() = default;

  Vec3f operator()(int x, int y) const {
    return data_[y % height_][x % width_];
  }

  int width_;
  int height_;

 protected:
  std::vector<std::vector<Vec3f>> data_;
  const std::string path_;
};