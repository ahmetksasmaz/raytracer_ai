#include "Scene.hpp"

void Scene::AveragingFilterAlgorithm(
    std::vector<std::vector<Vec3f>>& image_sampled_data,
    std::vector<Vec3f>& image_data) {
  for (int i = 0; i < image_data.size(); i++) {
    Vec3f sum{0.0f, 0.0f, 0.0f};
    for (int j = 0; j < image_sampled_data[i].size(); j++) {
      sum += image_sampled_data[i][j];
    }
    image_data[i] = sum / image_sampled_data[i].size();
  }
}