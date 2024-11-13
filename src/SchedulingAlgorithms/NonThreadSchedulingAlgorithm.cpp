#include "Scene.hpp"
#include "Timer.hpp"

void Scene::NonThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {
#ifdef DEBUG
  std::cout << "Camera resolution " << camera->image_height_ << "x"
            << camera->image_width_ << std::endl;
#endif
  for (int y = 0; y < camera->image_height_; ++y) {
    for (int x = 0; x < camera->image_width_; ++x) {
#ifdef DEBUG
      std::cout << "Tracing ray for index " << x << "," << y << std::endl;
#endif

      std::vector<Ray> rays = camera->GenerateRay({x, y});
#ifdef DEBUG
      std::cout << "Generated ray is " << "[" << ray.origin_.x << ray.origin_.y
                << ray.origin_.z << "]"
                << ","
                   "["
                << ray.direction_.x << ray.direction_.y << ray.direction_.z
                << "]" << std::endl;
#endif
      for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                           y * camera->image_width_ + x, ray_index);
        const Vec3f pixel_value =
            ray_tracing_algorithm_(rays[ray_index], nullptr,
                                   max_recursion_depth_, max_recursion_depth_);
#ifdef DEBUG
        std::cout << "Pixel value is " << "(" << pixel_value.x << pixel_value.y
                  << pixel_value.z << ")" << std::endl;
#endif
        camera->UpdateSampledPixelValue({x, y}, pixel_value);
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                           y * camera->image_width_ + x, ray_index);
      }
    }
  }
}