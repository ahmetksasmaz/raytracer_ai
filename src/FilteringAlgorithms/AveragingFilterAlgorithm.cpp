#include "Scene.hpp"

void Scene::AveragingFilterAlgorithm(Vec5f* image_sampled_data, int image_width,
                                     int image_height, int sample,
                                     Vec3f* image_data) {
  for (int i = 0; i < image_height; i++) {
    for (int j = 0; j < image_width; j++) {
      Vec3f sum{0.0f, 0.0f, 0.0f};
      for (int k = 0; k < sample; k++) {
        Vec5f packet = image_sampled_data[(i * image_width + j) * sample + k];
        Vec3f pixel_value = Vec3f{packet.x, packet.y, packet.z};
        sum += pixel_value;
      }

      image_data[i * image_width + j] = sum / sample;
    }
  }
}