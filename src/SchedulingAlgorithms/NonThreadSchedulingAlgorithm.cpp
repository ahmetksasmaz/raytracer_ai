#include "Scene.hpp"
#include "Timer.hpp"

void Scene::NonThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {
  for (int y = 0; y < camera->image_height_; ++y) {
    for (int x = 0; x < camera->image_width_; ++x) {

      std::vector<Ray> rays = camera->GenerateRay({x, y});
      for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                           y * camera->image_width_ + x, ray_index);
        const Vec3f pixel_value =
            ray_tracing_algorithm_(rays[ray_index], nullptr,
                                   max_recursion_depth_, max_recursion_depth_);
        camera->UpdateSampledPixelValue({x, y}, pixel_value, ray_index,
                                        rays[ray_index].diff_);
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                           y * camera->image_width_ + x, ray_index);
      }
    }
  }
}