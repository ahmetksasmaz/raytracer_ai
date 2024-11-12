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
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                           index.second * camera->image_width_ + index.first,
                           0);
        Ray ray = camera->GenerateRay({index.first, index.second});
        const Vec3f pixel_value = ray_tracing_algorithm_(
            ray, nullptr, max_recursion_depth_, max_recursion_depth_);
        camera->UpdatePixelValue({index.first, index.second}, pixel_value);
        if (timer.configuration_.timer_.ray_tracing_)
          timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                           index.second * camera->image_width_ + index.first,
                           0);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }
}