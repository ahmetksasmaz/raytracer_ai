#include "Scene.hpp"

void Scene::NonThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera) {
  for (int y = 0; y < camera->image_height_; ++y) {
    for (int x = 0; x < camera->image_width_; ++x) {
      const Ray ray = camera->GenerateRay({x, y});
      const Vec3f pixel_value = ray_tracing_algorithm_(
          ray, nullptr, max_recursion_depth_, max_recursion_depth_);
      camera->UpdatePixelValue({x, y}, pixel_value);
    }
  }
}