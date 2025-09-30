#pragma once
#include "../extern/parser.h"

using namespace parser;

class BaseImage {
 public:
  BaseImage(const std::string& path);
  virtual ~BaseImage() = default;

  Vec3uc operator()(int x, int y) const {
    return data_[y % height_][x % width_];
  }

 protected:
  int width_;
  int height_;
  std::vector<std::vector<Vec3uc>> data_;
  const std::string path_;
};