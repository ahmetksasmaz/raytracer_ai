#include "Scene.hpp"
#include "Timer.hpp"

void Scene::NonThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {
      std::shared_ptr<PathTracerSettings> path_tracer_settings = std::make_shared<PathTracerSettings>();
      path_tracer_settings->max_recursion_depth = camera->max_recursion_depth_;
      path_tracer_settings->min_recursion_depth = camera->min_recursion_depth_;
      path_tracer_settings->splitting_factor = camera->splitting_factor_;
      path_tracer_settings->importance_sampling_enabled = camera->importance_sampling_enabled_;
      path_tracer_settings->nee_enabled = camera->nee_enabled_;
      path_tracer_settings->mis_balance_enabled = camera->mis_balance_enabled_;
      path_tracer_settings->russian_roulette_enabled = camera->russian_roulette_enabled_;

  for (int y = 0; y < camera->image_height_; ++y) {
    for (int x = 0; x < camera->image_width_; ++x) {

      std::vector<Ray> rays = camera->GenerateRay({x, y});
      for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                           y * camera->image_width_ + x, ray_index);
        const Vec3f pixel_value =
        camera->path_tracing_enabled_ ?
        path_tracing_algorithm_(rays[ray_index], nullptr, 0, path_tracer_settings)
            : ray_tracing_algorithm_(
              rays[ray_index], nullptr, camera->max_recursion_depth_,
              camera->max_recursion_depth_);
        camera->UpdateSampledPixelValue({x, y}, pixel_value, ray_index,
                                        rays[ray_index].diff_);
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                           y * camera->image_width_ + x, ray_index);
      }
    }
  }
}