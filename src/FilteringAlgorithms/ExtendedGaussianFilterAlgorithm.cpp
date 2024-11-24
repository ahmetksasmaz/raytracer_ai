#include "Scene.hpp"

void Scene::ExtendedGaussianFilterAlgorithm(Vec5f* image_sampled_data,
                                            int image_width, int image_height,
                                            int sample, Vec3f* image_data) {
  int gaussian_kernel_size = configuration_.sampling_.gaussian_kernel_size_ / 2;
  for (int i = 0; i < image_height; i++) {
    for (int j = 0; j < image_width; j++) {
      Vec3f sum{0.0f, 0.0f, 0.0f};
      float sum_of_weights = 0.0;

      for (int a = -gaussian_kernel_size; a <= gaussian_kernel_size; a++) {
        for (int b = -gaussian_kernel_size; b <= gaussian_kernel_size; b++) {
          if (i + a < 0 || i + a >= image_height || j + b < 0 ||
              j + b >= image_width) {
            continue;
          }
          for (int k = 0; k < sample; k++) {
            Vec5f packet =
                image_sampled_data[((i + a) * image_width + (j + b)) * sample +
                                   k];
            Vec3f pixel_value = Vec3f{packet.x, packet.y, packet.z};
            Vec2f diff = Vec2f{packet.w, packet.t};
            float weight = gaussian_kernel_weight(
                (diff + Vec2f{float(a), float(b)}) /
                    (configuration_.sampling_.gaussian_kernel_size_),
                configuration_.sampling_.gaussian_kernel_sigma_);
            sum_of_weights += weight;
            sum += pixel_value * weight;
          }
        }
      }
      image_data[i * image_width + j] = sum / sum_of_weights;
    }
  }
}