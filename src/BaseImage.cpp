#include "BaseImage.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"
#include "STBExporter.hpp"

BaseImage::BaseImage(const std::string& path) : path_(path) {
    unsigned char* image =
        stbi_load(path.c_str(), &width_, &height_, nullptr, 3);

    if (image) {
      mipmapping_.push_back(std::vector<std::vector<Vec3f>>(height_, std::vector<Vec3f>(width_)));
      for (int i = 0; i < height_; i++) {
        for (int j = 0; j < width_; j++) {
          mipmapping_[0][i][j] = Vec3f{FP_PRECISION(image[3 * (i * width_ + j) + 0]),
                               FP_PRECISION(image[3 * (i * width_ + j) + 1]),
                               FP_PRECISION(image[3 * (i * width_ + j) + 2])};
        }
      }

      free(image);
    } else {
      throw std::runtime_error("Error: The image " + path_ +
                               " cannot be loaded.");
    }

    // Initialize first mipmapping level
    int current_width = width_;
    int current_height = height_;
    int current_depth = 0;
    do{
      current_width /= 2;
      current_height /= 2;
      current_depth++;
      mipmapping_.push_back(std::vector<std::vector<Vec3f>>(current_height, std::vector<Vec3f>(current_width)));

      for(int i = 0; i < current_height; i++) {
        for(int j = 0; j < current_width; j++) {
          Vec3f avg_color = (mipmapping_[current_depth-1][2*i][2*j] + mipmapping_[current_depth-1][2*i][2*j+1] + mipmapping_[current_depth-1][2*i+1][2*j] + mipmapping_[current_depth-1][2*i+1][2*j+1]) / 4.0;
          mipmapping_[current_depth][i][j] = avg_color;
        }
      }
    }
    while(current_height > 1 && current_width > 1);

    // Save every mipmapping level into png files for debugging
    // for(int level = 0; level < mipmapping_.size(); level++) {
    //   int level_width = width_ >> level;
    //   int level_height = height_ >> level;
    //   unsigned char* level_image = (unsigned char*)malloc(level_width * level_height * 3);
    //   for(int i = 0; i < level_height; i++) {
    //     for(int j = 0; j < level_width; j++) {
    //       level_image[3 * (i * level_width + j) + 0] = static_cast<unsigned char>(mipmapping_[level][i][j].x * 255.0);
    //       level_image[3 * (i * level_width + j) + 1] = static_cast<unsigned char>(mipmapping_[level][i][j].y * 255.0);
    //       level_image[3 * (i * level_width + j) + 2] = static_cast<unsigned char>(mipmapping_[level][i][j].z * 255.0);
    //     }
    //   }
    //   auto exporter_ = std::make_shared<STBExporter>();
    //   exporter_->Export("mipmapping_level_" + std::to_string(level) + ".png", level_image, level_width, level_height);
    //   free(level_image);
    // }
  }