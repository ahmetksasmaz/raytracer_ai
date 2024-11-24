#include "Scene.hpp"

void Scene::GaussianFilterAlgorithm(
    std::vector<std::vector<std::vector<std::pair<Vec3f, Vec2f>>>>&
        image_sampled_data,
    std::vector<Vec3f>& image_data) {
  for (int i = 0; i < image_sampled_data.size(); i++) {
    for (int j = 0; j < image_sampled_data[i].size(); j++) {
      Vec3f sum{0.0f, 0.0f, 0.0f};
      float sum_of_weights = 0.0;
      for (int k = 0; k < image_sampled_data[i][j].size(); k++) {
        float weight = gaussian_kernel_weight(
            image_sampled_data[i][j][k].second,
            configuration_.sampling_.gaussian_kernel_sigma_);
        sum_of_weights += weight;
        sum += image_sampled_data[i][j][k].first * weight;
      }
      image_data[i * image_sampled_data[0].size() + j] = sum / sum_of_weights;
    }
  }
}