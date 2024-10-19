#include <algorithm>

#include "Scene.hpp"

void Scene::ClampToneMappingAlgorithm(
    const std::vector<Vec3f>& image_data,
    std::vector<unsigned char>& tonemapped_image_data) {
  for (size_t i = 0; i < image_data.size(); i++) {
    tonemapped_image_data[i * 3 + 0] = static_cast<unsigned char>(
        std::max(0.0f, std::min(255.0f, image_data[i].x)));
    tonemapped_image_data[i * 3 + 1] = static_cast<unsigned char>(
        std::max(0.0f, std::min(255.0f, image_data[i].y)));
    tonemapped_image_data[i * 3 + 2] = static_cast<unsigned char>(
        std::max(0.0f, std::min(255.0f, image_data[i].z)));
  }
}