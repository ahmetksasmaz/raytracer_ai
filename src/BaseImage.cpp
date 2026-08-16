#include "BaseImage.hpp"

// Decoding goes through rt_imageio, which is the only translation unit that
// instantiates tinyexr and stb. This file used to define TINYEXR_IMPLEMENTATION
// and STB_IMAGE_IMPLEMENTATION itself, which worked only as long as the
// renderer was one binary; with a dozen executables linking the same libraries
// it is a duplicate-symbol link error.
#include "ImageIO/ImageIO.hpp"

BaseImage::BaseImage(const std::string& path) : path_(path) {

    std::string extension = path.substr(path.find_last_of('.') + 1);
    std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
    if(extension == "exr" || extension == "hdr"){
      image_io::ImagePlanes image;
      std::string error;
      if (!image_io::ReadMultiChannelEXR(path, &image, &error)) {
        throw std::runtime_error("Error: The image " + path_ +
                                 " cannot be loaded: " + error);
      }

      // Named channels where present; otherwise the first three planes, which
      // is what an EXR written by something else is likely to hold.
      int channel[3];
      const char* names[3] = {"R", "G", "B"};
      for (int c = 0; c < 3; c++) {
        channel[c] = image.IndexOf(names[c]);
        if (channel[c] < 0) channel[c] = c < image.ChannelCount() ? c : 0;
      }
      if (image.ChannelCount() == 0) {
        throw std::runtime_error("Error: The image " + path_ +
                                 " has no channels.");
      }

      width_ = image.width;
      height_ = image.height;
      mipmapping_.push_back(std::vector<std::vector<Vec3f>>(height_, std::vector<Vec3f>(width_)));
      for (int i = 0; i < height_; i++) {
        for (int j = 0; j < width_; j++) {
          const size_t index = static_cast<size_t>(i) * width_ + j;
          mipmapping_[0][i][j] = Vec3f{
              FP_PRECISION(image.planes[channel[0]][index]),
              FP_PRECISION(image.planes[channel[1]][index]),
              FP_PRECISION(image.planes[channel[2]][index])};
        }
      }
    }
    else{
      int channels = 3;
      std::vector<unsigned char> image;
      std::string error;
      if (!image_io::ReadLDR(path, &width_, &height_, &channels, &image,
                             &error)) {
        throw std::runtime_error("Error: The image " + path_ +
                                " cannot be loaded.");
      }

      mipmapping_.push_back(std::vector<std::vector<Vec3f>>(height_, std::vector<Vec3f>(width_)));
      for (int i = 0; i < height_; i++) {
        for (int j = 0; j < width_; j++) {
          const size_t index = (static_cast<size_t>(i) * width_ + j) * 3;
          mipmapping_[0][i][j] = Vec3f{FP_PRECISION(image[index + 0]),
                                       FP_PRECISION(image[index + 1]),
                                       FP_PRECISION(image[index + 2])};
        }
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