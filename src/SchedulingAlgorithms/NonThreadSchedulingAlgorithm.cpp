#include "Scene.hpp"

void Scene::NonThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera) {
#ifdef DEBUG
  std::cout << "Camera resolution " << camera->image_height_ << "x"
            << camera->image_width_ << std::endl;
#endif
  for (int y = 0; y < camera->image_height_; ++y) {
    for (int x = 0; x < camera->image_width_; ++x) {
#ifdef DEBUG
      std::cout << "Tracing ray for index " << x << "," << y << std::endl;
#endif
      Ray ray = camera->GenerateRay({x, y});
#ifdef DEBUG
      std::cout << "Generated ray is " << "[" << ray.origin_.x << ray.origin_.y
                << ray.origin_.z << "]"
                << ","
                   "["
                << ray.direction_.x << ray.direction_.y << ray.direction_.z
                << "]" << std::endl;
#endif
      const Vec3f pixel_value = ray_tracing_algorithm_(
          ray, nullptr, max_recursion_depth_, max_recursion_depth_);
#ifdef DEBUG
      std::cout << "Pixel value is " << "(" << pixel_value.x << pixel_value.y
                << pixel_value.z << ")" << std::endl;
#endif
      camera->UpdatePixelValue({x, y}, pixel_value);
    }
  }
}