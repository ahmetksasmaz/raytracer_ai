#pragma once
#include "../extern/parser.h"
#include "Helper.hpp"

using namespace parser;

class BaseImage {
 public:
  BaseImage(const std::string& path);
  virtual ~BaseImage() = default;

  Vec3f operator()(int x, int y, int level = 0) const {
    if (level >= static_cast<int>(mipmapping_.size())) {
      level = static_cast<int>(mipmapping_.size()) - 1;
    }
    return mipmapping_[level][(y) % (height_ >> level)][(x) % (width_ >> level)];
  }

  int width_;
  int height_;

 protected:
  std::vector<std::vector<std::vector<Vec3f>>> mipmapping_;
  const std::string path_;
};