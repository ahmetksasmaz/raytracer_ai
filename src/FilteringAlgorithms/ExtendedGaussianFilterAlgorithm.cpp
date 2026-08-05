#include "Scene.hpp"

// Sigma is in PIXELS. At the previous value of 0.1 a sample only 0.3 px from the
// pixel centre was weighted about ten thousand times less than one exactly at
// the centre, so nearly every ray that was traced and paid for contributed
// essentially nothing, and the 3x3 neighbourhood contributed nothing at all.
// Around half a pixel is the usual choice: wide enough to actually use the
// samples, narrow enough not to visibly blur.
static constexpr FP_PRECISION kGaussianKernelSigma = 0.5;
static constexpr int kGaussianKernelSize = 3;

void Scene::ExtendedGaussianFilterAlgorithm(Vec5f* image_sampled_data,
                                            int image_width, int image_height,
                                            int sample, Vec3f* image_data) {
  int gaussian_kernel_half = kGaussianKernelSize / 2;
  for (int i = 0; i < image_height; i++) {
    for (int j = 0; j < image_width; j++) {
      Vec3f sum{0.0f, 0.0f, 0.0f};
      FP_PRECISION sum_of_weights = 0.0;

      for (int a = -gaussian_kernel_half; a <= gaussian_kernel_half; a++) {
        for (int b = -gaussian_kernel_half; b <= gaussian_kernel_half; b++) {
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
            // diff is the sample's position within its own pixel, in [0,1).
            // Subtracting 0.5 centres it on that pixel, and adding the integer
            // neighbour offset places it relative to the pixel being
            // reconstructed. diff.x runs along columns (b), diff.y along rows
            // (a). The old expression divided the whole thing by the kernel
            // size, which shrank the neighbour offsets to a third of a pixel
            // and left the centring wrong.
            const Vec2f centered_offset{diff.x - 0.5 + static_cast<FP_PRECISION>(b),
                                        diff.y - 0.5 + static_cast<FP_PRECISION>(a)};
            FP_PRECISION weight =
                gaussian_kernel_weight(centered_offset, kGaussianKernelSigma);
            sum_of_weights += weight;
            sum += pixel_value * weight;
          }
        }
      }
      image_data[i * image_width + j] = sum / sum_of_weights;
    }
  }
}