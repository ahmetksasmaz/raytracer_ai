#pragma once

#include <fstream>

#include "../extern/json.hpp"

enum class RayTracingAlgorithm {
  kDefault = 0,
  kRecursive = 1,
  kBest = 1,
  kMax = 2
};

enum class SchedulingAlgorithm {
  kNonThread = 0,
  kThreadQueue = 1,
  kBest = 1,
  kMax = 1
};

enum class ToneMappingAlgorithm { kClamp = 0, kBest = 0, kMax = 0 };

enum class ExporterType { kPPM = 0, kSTB = 1, kBest = 1, kMax = 1 };

struct Configuration {
  struct Shading {
    bool ambient_ = true;
    bool diffuse_ = true;
    bool specular_ = true;
  } shading_;

  struct Materials {
    bool mirror_ = true;
    bool conductor_ = true;
    bool dielectric_ = true;
  } materials_;

  struct Strategies {
    RayTracingAlgorithm ray_tracing_algorithm_ = RayTracingAlgorithm::kBest;
    SchedulingAlgorithm scheduling_algorithm_ = SchedulingAlgorithm::kBest;
    ToneMappingAlgorithm tone_mapping_algorithm_ = ToneMappingAlgorithm::kBest;
    ExporterType exporter_type_ = ExporterType::kBest;
  } strategies_;

  struct Acceleration {
    bool bvh_low_level_ = true;
    bool bvh_high_level_ = true;
  } acceleration_;

  void ParseFromFile(const std::string& filename) {
    using json = nlohmann::json;

    std::ifstream f(filename);
    json data = json::parse(f);

    data.at("shading").at("ambient").get_to(shading_.ambient_);
    data.at("shading").at("diffuse").get_to(shading_.diffuse_);
    data.at("shading").at("specular").get_to(shading_.specular_);

    data.at("materials").at("mirror").get_to(materials_.mirror_);
    data.at("materials").at("conductor").get_to(materials_.conductor_);
    data.at("materials").at("dielectric").get_to(materials_.dielectric_);

    std::string ray_tracing_algorithm;
    data.at("strategies").at("ray_tracing").get_to(ray_tracing_algorithm);
    if (ray_tracing_algorithm == "default") {
      strategies_.ray_tracing_algorithm_ = RayTracingAlgorithm::kDefault;
    } else if (ray_tracing_algorithm == "recursive") {
      strategies_.ray_tracing_algorithm_ = RayTracingAlgorithm::kRecursive;
    }

    std::string scheduling_algorithm;
    data.at("strategies").at("scheduling").get_to(scheduling_algorithm);
    if (scheduling_algorithm == "non_thread") {
      strategies_.scheduling_algorithm_ = SchedulingAlgorithm::kNonThread;
    } else if (scheduling_algorithm == "thread_queue") {
      strategies_.scheduling_algorithm_ = SchedulingAlgorithm::kThreadQueue;
    }

    std::string tone_mapping_algorithm;
    data.at("strategies").at("tone_mapping").get_to(tone_mapping_algorithm);
    if (tone_mapping_algorithm == "clamp") {
      strategies_.tone_mapping_algorithm_ = ToneMappingAlgorithm::kClamp;
    }

    std::string exporter_type;
    data.at("strategies").at("exporter").get_to(exporter_type);
    if (exporter_type == "ppm") {
      strategies_.exporter_type_ = ExporterType::kPPM;
    } else if (exporter_type == "stb") {
      strategies_.exporter_type_ = ExporterType::kSTB;
    }

    data.at("acceleration")
        .at("bvh_low_level")
        .get_to(acceleration_.bvh_low_level_);
    data.at("acceleration")
        .at("bvh_high_level")
        .get_to(acceleration_.bvh_high_level_);
  }
};