#pragma once
#include "../extern/parser.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"

using namespace parser;

class BaseImage {
 public:
  BaseImage(const std::string& path) : path_(path) {
    unsigned char* image =
        stbi_load(path.c_str(), &width_, &height_, nullptr, 3);

    if (image) {
      data_.resize(height_);
      for (int i = 0; i < height_; i++) {
        data_[i].resize(width_);
        for (int j = 0; j < width_; j++) {
          data_[i][j] = Vec3uc{image[3 * (i * width_ + j) + 0],
                               image[3 * (i * width_ + j) + 1],
                               image[3 * (i * width_ + j) + 2]};
        }
      }

      free(image);
    } else {
      throw std::runtime_error("Error: The image " + path_ +
                               " cannot be loaded.");
    }
  }
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