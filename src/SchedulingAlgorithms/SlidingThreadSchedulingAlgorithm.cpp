#include <mutex>
#include <queue>
#include <thread>

#include "Scene.hpp"
#include "Timer.hpp"

void Scene::SlidingThreadSchedulingAlgorithm(
    const std::shared_ptr<BaseCamera> camera, int camera_index) {


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
              if (timer.configuration_.timer_.ray_tracing_)
                timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                                 y * camera->image_width_ + x,
                                 ray_index);
              const Vec3f pixel_value = ray_tracing_algorithm_(
                  rays[ray_index], nullptr, max_recursion_depth_,
                  max_recursion_depth_);
              camera->UpdateSampledPixelValue({x, y},
                                              pixel_value, ray_index,
                                              rays[ray_index].diff_);
              if (timer.configuration_.timer_.ray_tracing_)
                timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                                 y * camera->image_width_ + x,
                                 ray_index);
            }
          });
        }
        // Join threads
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
            if (timer.configuration_.timer_.ray_tracing_)
              timer.AddTimeLog(Section::kRayTracing, Event::kStart, camera_index,
                               y * camera->image_width_ + x,
                               ray_index);
            const Vec3f pixel_value = ray_tracing_algorithm_(
                rays[ray_index], nullptr, max_recursion_depth_,
                max_recursion_depth_);
            camera->UpdateSampledPixelValue({x, y},
                                            pixel_value, ray_index,
                                            rays[ray_index].diff_);
            if (timer.configuration_.timer_.ray_tracing_)
              timer.AddTimeLog(Section::kRayTracing, Event::kEnd, camera_index,
                               y * camera->image_width_ + x,
                               ray_index);
          }
        });
    }
    // Join threads
    for (size_t i = 0; i < threads.size(); i++)
    {
      threads[i].join();
    }

  }

  // std::thread status_thread([&]() {
  //   while (true) {
  //     std::this_thread::sleep_for(std::chrono::seconds(1));
  //     std::lock_guard<std::mutex> lock(queue_mutex);
  //     float progress =
  //         1.0f - static_cast<float>(queue.size()) /
  //                    (camera->image_width_ * camera->image_height_);
  //     std::cout << "Progress: " << progress * 100 << "%" << std::endl;
  //     if (queue.empty()) {
  //       break;
  //     }
  //   }
  // });

  // status_thread.join();

  }
}