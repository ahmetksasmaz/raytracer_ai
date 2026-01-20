#include <mutex>
#include <queue>
#include <thread>

#include "Scene.hpp"
#include "Timer.hpp"

void Scene::BlockDivideThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {
      std::shared_ptr<PathTracerSettings> path_tracer_settings = std::make_shared<PathTracerSettings>();
      path_tracer_settings->max_recursion_depth = camera->max_recursion_depth_;
      path_tracer_settings->min_recursion_depth = camera->min_recursion_depth_;
      path_tracer_settings->splitting_factor = camera->splitting_factor_;
      path_tracer_settings->importance_sampling_enabled = camera->importance_sampling_enabled_;
      path_tracer_settings->nee_enabled = camera->nee_enabled_;
      path_tracer_settings->mis_balance_enabled = camera->mis_balance_enabled_;
      path_tracer_settings->russian_roulette_enabled = camera->russian_roulette_enabled_;


  auto processor_count = std::thread::hardware_concurrency();
  processor_count = processor_count > 0 ? processor_count : 8;

  std::vector<std::thread> threads;

  std::vector<std::queue<std::pair<int, int>>> queues(processor_count);
  // Try to divide processor count as square as possible
  int block_rows = static_cast<int>(std::sqrt(processor_count));
  int block_cols = (processor_count + block_rows - 1) / block_rows;
    int block_width = (camera->image_width_ + block_cols - 1) / block_cols;
    int block_height = (camera->image_height_ + block_rows - 1) / block_rows;
    for (int by = 0; by < block_rows; ++by) {
        for (int bx = 0; bx < block_cols; ++bx) {
        int start_x = bx * block_width;
        int end_x = std::min(start_x + block_width, camera->image_width_-1);
        int start_y = by * block_height;
        int end_y = std::min(start_y + block_height, camera->image_height_-1);
        int queue_index = by * block_cols + bx;
        if (queue_index >= processor_count) {
            continue;
        }
        for (int y = start_y; y < end_y; ++y) {
            for (int x = start_x; x < end_x; ++x) {
            queues[queue_index].push({x, y});
            }
        }
        }
    }

  for (size_t i = 0; i < processor_count; i++) {
    threads.emplace_back([&, i]() {
    // threads.emplace_back([&]() {
      while (true) {
        if (queues[i].empty()) {
          break;
        }
        std::pair<int, int> index = queues[i].front();
        queues[i].pop();

        std::vector<Ray> rays =
            camera->GenerateRay({index.first, index.second});
        for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
          if (timer.configuration_.timer_.ray_tracing_)
            timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                             index.second * camera->image_width_ + index.first,
                             ray_index);
          const Vec3f pixel_value =
        camera->path_tracing_enabled_ ?
        path_tracing_algorithm_(rays[ray_index], nullptr, 0, path_tracer_settings)
            : ray_tracing_algorithm_(
              rays[ray_index], nullptr, camera->max_recursion_depth_,
              camera->max_recursion_depth_);
          camera->UpdateSampledPixelValue({index.first, index.second},
                                          pixel_value, ray_index,
                                          rays[ray_index].diff_);
          if (timer.configuration_.timer_.ray_tracing_)
            timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                             index.second * camera->image_width_ + index.first,
                             ray_index);
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}