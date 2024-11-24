#include <mutex>
#include <queue>
#include <thread>

#include "Scene.hpp"
#include "Timer.hpp"

void Scene::ThreadQueueSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {
  std::queue<std::pair<int, int>> queue;
  std::mutex queue_mutex;
  for (int y = 0; y < camera->image_height_; ++y) {
    for (int x = 0; x < camera->image_width_; ++x) {
      queue.push({x, y});
    }
  }

  auto processor_count = std::thread::hardware_concurrency();
  processor_count = processor_count > 0 ? processor_count : 8;

  std::vector<std::thread> threads;

  for (size_t i = 0; i < processor_count; i++) {
    threads.emplace_back([&]() {
      while (true) {
        std::pair<int, int> index;
        {
          std::lock_guard<std::mutex> lock(queue_mutex);
          if (queue.empty()) {
            break;
          }
          index = queue.front();
          queue.pop();
        }

        std::vector<Ray> rays =
            camera->GenerateRay({index.first, index.second});
        for (int ray_index = 0; ray_index < rays.size(); ray_index++) {
          if (timer.configuration_.timer_.ray_tracing_)
            timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                             index.second * camera->image_width_ + index.first,
                             ray_index);
          const Vec3f pixel_value = ray_tracing_algorithm_(
              rays[ray_index], nullptr, max_recursion_depth_,
              max_recursion_depth_);
          camera->UpdateSampledPixelValue({index.first, index.second},
                                          pixel_value, rays[ray_index].diff_);
          if (timer.configuration_.timer_.ray_tracing_)
            timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                             index.second * camera->image_width_ + index.first,
                             ray_index);
        }
      }
    });
  }

  std::thread status_thread([&]() {
    while (true) {
      std::this_thread::sleep_for(std::chrono::seconds(1));
      std::lock_guard<std::mutex> lock(queue_mutex);
      float progress =
          1.0f - static_cast<float>(queue.size()) /
                     (camera->image_width_ * camera->image_height_);
      std::cout << "Progress: " << progress * 100 << "%" << std::endl;
      if (queue.empty()) {
        break;
      }
    }
  });

  status_thread.join();

  for (auto& thread : threads) {
    thread.join();
  }
}