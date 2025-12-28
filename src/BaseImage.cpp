#include "BaseImage.hpp"

#define STB_IMAGE_IMPLEMENTATION
#include "../extern/stb_image.h"
#include "STBExtendedExporter.hpp"

#define TINYEXR_IMPLEMENTATION
#include "../extern/tinyexr.h"

BaseImage::BaseImage(const std::string& path) : path_(path) {

    std::string extension = path.substr(path.find_last_of('.') + 1);
    extension = std::to_lower(extension);
    if(extension == "exr" || extension == "hdr"){
      float* out;
      const char* err = NULL;

      int ret = LoadEXR(&out, &width_, &height_, path.c_str(), &err);

      if (ret != TINYEXR_SUCCESS) {
        throw std::runtime_error("Error: The image " + path_ +
                              " cannot be loaded.");
      } else {
        mipmapping_.push_back(std::vector<std::vector<Vec3f>>(height_, std::vector<Vec3f>(width_)));
        for (int i = 0; i < height_; i++) {
          for (int j = 0; j < width_; j++) {
            mipmapping_[0][i][j] = Vec3f{FP_PRECISION(out[3 * (i * width_ + j) + 0]),
                                FP_PRECISION(out[3 * (i * width_ + j) + 1]),
                                FP_PRECISION(out[3 * (i * width_ + j) + 2])};
          }
        }

        free(out);
      }
    }
    else{
      unsigned char * image = stbi_load(path.c_str(), &width_, &height_, nullptr, 3);

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
  }