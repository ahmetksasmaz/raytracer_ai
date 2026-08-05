#include <mutex>
#include <queue>
#include <thread>
#include <atomic>

#include "Scene.hpp"
#include "Timer.hpp"

constexpr int TILE_SIZE = 32;

void Scene::ThreadQueueSchedulingAlgorithm(
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
  
  int num_tiles_x = (camera->image_width_ + TILE_SIZE - 1) / TILE_SIZE;
  int num_tiles_y = (camera->image_height_ + TILE_SIZE - 1) / TILE_SIZE;
  int total_tiles = num_tiles_x * num_tiles_y;
  
  std::atomic<int> tile_counter(0);
  std::atomic<int> completed_pixels(0);
  int total_pixels = camera->image_width_ * camera->image_height_;

  auto processor_count = std::thread::hardware_concurrency();
  processor_count = processor_count > 0 ? processor_count : 8;

  std::vector<std::thread> threads;

  for (size_t i = 0; i < processor_count; i++) {
    threads.emplace_back([&, i]() {
      ThreadLocalRandomNumberGenerator = FastRandomNumberGenerator(std::hash<std::thread::id>{}(std::this_thread::get_id()) ^ (i * 12345));
      
      while (true) {
        int tile_idx = tile_counter.fetch_add(1);
        if (tile_idx >= total_tiles) break;
        
        int tile_x = tile_idx % num_tiles_x;
        int tile_y = tile_idx / num_tiles_x;
        int start_x = tile_x * TILE_SIZE;
        int start_y = tile_y * TILE_SIZE;
        int end_x = std::min(start_x + TILE_SIZE, camera->image_width_);
        int end_y = std::min(start_y + TILE_SIZE, camera->image_height_);
        
        for (int y = start_y; y < end_y; ++y) {
          for (int x = start_x; x < end_x; ++x) {
            std::vector<Ray> rays = camera->GenerateRay({x, y});
            for (int ray_index = 0; ray_index < static_cast<int>(rays.size()); ray_index++) {
              const Vec3f pixel_value =
                  camera->path_tracing_enabled_ ?
                  path_tracing_algorithm_(rays[ray_index], nullptr, 0, path_tracer_settings, PathState{})
                  : ray_tracing_algorithm_(
                      rays[ray_index], nullptr, camera->max_recursion_depth_,
                      camera->max_recursion_depth_);
              camera->UpdateSampledPixelValue({x, y}, pixel_value, ray_index,
                                              rays[ray_index].diff_);
            }
            completed_pixels.fetch_add(1);
          }
        }
      }
    });
  }

  std::thread status_thread([&]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      int completed = completed_pixels.load();
      FP_PRECISION progress = static_cast<FP_PRECISION>(completed) / total_pixels;
      std::cout << "Progress: " << progress * 100 << "%" << std::endl;
      if (completed >= total_pixels) {
        break;
      }
    }
  });

  for (auto& thread : threads) {
    thread.join();
  }
  
  status_thread.join();
}