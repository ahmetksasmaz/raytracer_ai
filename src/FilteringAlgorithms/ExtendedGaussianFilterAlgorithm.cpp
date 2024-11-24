#include "Scene.hpp"

void Scene::ExtendedGaussianFilterAlgorithm(
    std::vector<std::vector<std::vector<std::pair<Vec3f, Vec2f>>>>&
        image_sampled_data,
    std::vector<Vec3f>& image_data) {
  int gaussian_kernel_size = configuration_.sampling_.gaussian_kernel_size_ / 2;

  for (int i = 0; i < image_sampled_data.size(); i++) {
    for (int j = 0; j < image_sampled_data[i].size(); j++) {
      Vec3f sum{0.0f, 0.0f, 0.0f};
      float sum_of_weights = 0.0;

      for (int a = -gaussian_kernel_size; a <= gaussian_kernel_size; a++) {
        for (int b = -gaussian_kernel_size; b <= gaussian_kernel_size; b++) {
          if (i + a < 0 || i + a >= image_sampled_data.size() || j + b < 0 ||
              j + b >= image_sampled_data[i].size()) {
            continue;
          }
          for (int k = 0; k < image_sampled_data[i + a][j + b].size(); k++) {
            float weight = gaussian_kernel_weight(
                (image_sampled_data[i + a][j + b][k].second +
                 Vec2f{float(a), float(b)}) /
                    (configuration_.sampling_.gaussian_kernel_size_),
                configuration_.sampling_.gaussian_kernel_sigma_);
            sum_of_weights += weight;
            sum += image_sampled_data[i + a][j + b][k].first * weight;
          }
        }
      }
      image_data[i * image_sampled_data[0].size() + j] = sum / sum_of_weights;
    }
  }
}