#include "Scene.hpp"

// In pixels -- see ExtendedGaussianFilterAlgorithm.cpp for why 0.1 was far too
// narrow to be usable.
static constexpr FP_PRECISION kGaussianKernelSigma = 0.5;
static constexpr int kGaussianKernelSize = 3;

void Scene::GaussianFilterAlgorithm(Vec5f* image_sampled_data, int image_width,
                                    int image_height, int sample,
                                    Vec3f* image_data) {
  for (int i = 0; i < image_height; i++) {
    for (int j = 0; j < image_width; j++) {
      Vec3f sum{0.0f, 0.0f, 0.0f};
      FP_PRECISION sum_of_weights = 0.0;
      for (int k = 0; k < sample; k++) {
        Vec5f packet = image_sampled_data[(i * image_width + j) * sample + k];
        Vec3f pixel_value = Vec3f{packet.x, packet.y, packet.z};
        Vec2f diff = Vec2f{packet.w, packet.t};
        // Centre the in-pixel offset; this filter only ever looks at the pixel's
        // own samples, so there is no neighbour offset to add.
        const Vec2f centered_offset{diff.x - 0.5, diff.y - 0.5};
        FP_PRECISION weight =
            gaussian_kernel_weight(centered_offset, kGaussianKernelSigma);
        sum_of_weights += weight;
        sum += pixel_value * weight;
      }
      image_data[i * image_width + j] = sum / sum_of_weights;
    }
  }
}