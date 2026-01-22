#include <mutex>
#include <queue>
#include <thread>

#include "Scene.hpp"
#include "Timer.hpp"

void Scene::SlidingThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {
      PathTracerSettings path_tracer_settings;
      path_tracer_settings.max_recursion_depth = camera->max_recursion_depth_;
      path_tracer_settings.min_recursion_depth = camera->min_recursion_depth_;
      path_tracer_settings.splitting_factor = camera->splitting_factor_;
      path_tracer_settings.importance_sampling_enabled = camera->importance_sampling_enabled_;
      path_tracer_settings.nee_enabled = camera->nee_enabled_;
      path_tracer_settings.mis_balance_enabled = camera->mis_balance_enabled_;
      path_tracer_settings.russian_roulette_enabled = camera->russian_roulette_enabled_;
      path_tracer_settings.sample_max_val = camera->sample_max_val_;


  auto processor_count = std::thread::hardware_concurrency();
  processor_count = processor_count > 0 ? processor_count : 8;

  // For every row, divide pixels into processor count pixels
  int groups_per_row = camera->image_width_ / processor_count;
  int remaining_pixels = camera->image_width_ % processor_count;

  // For every row, iterate over groups, each thread process one pixel in the group
  for (int y = 0; y < camera->image_height_; ++y) {
    for (int group = 0; group < groups_per_row; ++group) {
      int start_x = group * processor_count;
      int end_x = start_x + processor_count;
      for (int x = start_x; x < end_x; ++x) {
        std::vector<std::thread> threads;
        for (size_t i = 0; i < processor_count; i++) {
          threads.emplace_back([&, x]() {
            std::vector<Ray> rays =
                camera->GenerateRay({x, y});
            for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
              const Vec3f pixel_value =
        camera->path_tracing_enabled_ ?
        path_tracing_algorithm_(rays[ray_index], nullptr, 0, path_tracer_settings, {1.0f, 1.0f, 1.0f})
            : ray_tracing_algorithm_(
              rays[ray_index], nullptr, camera->max_recursion_depth_,
              camera->max_recursion_depth_);
              camera->UpdateSampledPixelValue({x, y},
                                              pixel_value, ray_index,
                                              rays[ray_index].diff_);
            }
          });
        }
        for (size_t i = 0; i < threads.size(); i++)
        {
          threads[i].join();
        }
        
      }
    }
    // Distribute remaining pixels to the last group
    for (int x = camera->image_width_ - remaining_pixels; x < camera->image_width_; ++x) {
      std::vector<std::thread> threads;
      for (size_t i = 0; i < processor_count; i++) {
        threads.emplace_back([&, x]() {
          std::vector<Ray> rays =
              camera->GenerateRay({x, y});
          for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
            const Vec3f pixel_value =
        camera->path_tracing_enabled_ ?
        path_tracing_algorithm_(rays[ray_index], nullptr, 0, path_tracer_settings, {1.0f, 1.0f, 1.0f})
            : ray_tracing_algorithm_(
              rays[ray_index], nullptr, camera->max_recursion_depth_,
              camera->max_recursion_depth_);
            camera->UpdateSampledPixelValue({x, y},
                                            pixel_value, ray_index,
                                            rays[ray_index].diff_);
          }
        });
    }
    for (size_t i = 0; i < threads.size(); i++)
    {
      threads[i].join();
    }

  }

  }
}