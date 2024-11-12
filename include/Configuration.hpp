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

enum class SamplingAlgorithm {
  kUniform = 0,
  kRandom = 1,
  kJittered = 2,
  kMultiJittered = 3,
  kHalton = 4,
  kHammersley = 5,
  kBest = 5,
  kMax = 5
};

enum class FilteringAlgorithm {
  kBox = 0,
  kGaussian = 1,
  kTriangle = 2,
  kMitchell = 3,
  kLanczos = 4,
  kBest = 1,
  kMax = 4
};

enum class ToneMappingAlgorithm { kClamp = 0, kBest = 0, kMax = 0 };

enum class ExporterType { kPPM = 0, kSTB = 1, kBest = 1, kMax = 1 };

struct Configuration {
  struct Sampling {
    SamplingAlgorithm pixel_sampling_ = SamplingAlgorithm::kBest;
    SamplingAlgorithm aperture_sampling_ = SamplingAlgorithm::kBest;
    SamplingAlgorithm glossy_sampling_ = SamplingAlgorithm::kBest;
    SamplingAlgorithm area_light_sampling_ = SamplingAlgorithm::kBest;

    FilteringAlgorithm pixel_filtering_ = FilteringAlgorithm::kBest;
    FilteringAlgorithm glossy_filtering_ = FilteringAlgorithm::kBest;

    float pixel_sampling_ratio_ = 1.0f;
    float aperture_sampling_ratio_ = 1.0f;
    float glossy_sampling_ratio_ = 1.0f;
    float area_light_sampling_ratio_ = 1.0f;
  } sampling_;

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

    std::string sampling_algorithm;
    data.at("sampling").at("pixel_sampling").get_to(sampling_algorithm);
    if (sampling_algorithm == "uniform") {
      sampling_.pixel_sampling_ = SamplingAlgorithm::kUniform;
    } else if (sampling_algorithm == "random") {
      sampling_.pixel_sampling_ = SamplingAlgorithm::kRandom;
    } else if (sampling_algorithm == "jittered") {
      sampling_.pixel_sampling_ = SamplingAlgorithm::kJittered;
    } else if (sampling_algorithm == "multi_jittered") {
      sampling_.pixel_sampling_ = SamplingAlgorithm::kMultiJittered;
    } else if (sampling_algorithm == "halton") {
      sampling_.pixel_sampling_ = SamplingAlgorithm::kHalton;
    } else if (sampling_algorithm == "hammersley") {
      sampling_.pixel_sampling_ = SamplingAlgorithm::kHammersley;
    }

    data.at("sampling").at("aperture_sampling").get_to(sampling_algorithm);
    if (sampling_algorithm == "uniform") {
      sampling_.aperture_sampling_ = SamplingAlgorithm::kUniform;
    } else if (sampling_algorithm == "random") {
      sampling_.aperture_sampling_ = SamplingAlgorithm::kRandom;
    } else if (sampling_algorithm == "jittered") {
      sampling_.aperture_sampling_ = SamplingAlgorithm::kJittered;
    } else if (sampling_algorithm == "multi_jittered") {
      sampling_.aperture_sampling_ = SamplingAlgorithm::kMultiJittered;
    } else if (sampling_algorithm == "halton") {
      sampling_.aperture_sampling_ = SamplingAlgorithm::kHalton;
    } else if (sampling_algorithm == "hammersley") {
      sampling_.aperture_sampling_ = SamplingAlgorithm::kHammersley;
    }

    data.at("sampling").at("glossy_sampling").get_to(sampling_algorithm);
    if (sampling_algorithm == "uniform") {
      sampling_.glossy_sampling_ = SamplingAlgorithm::kUniform;
    } else if (sampling_algorithm == "random") {
      sampling_.glossy_sampling_ = SamplingAlgorithm::kRandom;
    } else if (sampling_algorithm == "jittered") {
      sampling_.glossy_sampling_ = SamplingAlgorithm::kJittered;
    } else if (sampling_algorithm == "multi_jittered") {
      sampling_.glossy_sampling_ = SamplingAlgorithm::kMultiJittered;
    } else if (sampling_algorithm == "halton") {
      sampling_.glossy_sampling_ = SamplingAlgorithm::kHalton;
    } else if (sampling_algorithm == "hammersley") {
      sampling_.glossy_sampling_ = SamplingAlgorithm::kHammersley;
    }

    data.at("sampling").at("area_light_sampling").get_to(sampling_algorithm);
    if (sampling_algorithm == "uniform") {
      sampling_.area_light_sampling_ = SamplingAlgorithm::kUniform;
    } else if (sampling_algorithm == "random") {
      sampling_.area_light_sampling_ = SamplingAlgorithm::kRandom;
    } else if (sampling_algorithm == "jittered") {
      sampling_.area_light_sampling_ = SamplingAlgorithm::kJittered;
    } else if (sampling_algorithm == "multi_jittered") {
      sampling_.area_light_sampling_ = SamplingAlgorithm::kMultiJittered;
    } else if (sampling_algorithm == "halton") {
      sampling_.area_light_sampling_ = SamplingAlgorithm::kHalton;
    } else if (sampling_algorithm == "hammersley") {
      sampling_.area_light_sampling_ = SamplingAlgorithm::kHammersley;
    }

    std::string filtering_algorithm;
    data.at("sampling").at("pixel_filtering").get_to(filtering_algorithm);
    if (filtering_algorithm == "box") {
      sampling_.pixel_filtering_ = FilteringAlgorithm::kBox;
    } else if (filtering_algorithm == "gaussian") {
      sampling_.pixel_filtering_ = FilteringAlgorithm::kGaussian;
    } else if (filtering_algorithm == "triangle") {
      sampling_.pixel_filtering_ = FilteringAlgorithm::kTriangle;
    } else if (filtering_algorithm == "mitchell") {
      sampling_.pixel_filtering_ = FilteringAlgorithm::kMitchell;
    } else if (filtering_algorithm == "lanczos") {
      sampling_.pixel_filtering_ = FilteringAlgorithm::kLanczos;
    }

    data.at("sampling").at("glossy_filtering").get_to(filtering_algorithm);
    if (filtering_algorithm == "box") {
      sampling_.glossy_filtering_ = FilteringAlgorithm::kBox;
    } else if (filtering_algorithm == "gaussian") {
      sampling_.glossy_filtering_ = FilteringAlgorithm::kGaussian;
    } else if (filtering_algorithm == "triangle") {
      sampling_.glossy_filtering_ = FilteringAlgorithm::kTriangle;
    } else if (filtering_algorithm == "mitchell") {
      sampling_.glossy_filtering_ = FilteringAlgorithm::kMitchell;
    } else if (filtering_algorithm == "lanczos") {
      sampling_.glossy_filtering_ = FilteringAlgorithm::kLanczos;
    }

    data.at("sampling")
        .at("pixel_sampling_ratio")
        .get_to(sampling_.pixel_sampling_ratio_);
    data.at("sampling")
        .at("aperture_sampling_ratio")
        .get_to(sampling_.aperture_sampling_ratio_);
    data.at("sampling")
        .at("glossy_sampling_ratio")
        .get_to(sampling_.glossy_sampling_ratio_);
    data.at("sampling")
        .at("area_light_sampling_ratio")
        .get_to(sampling_.area_light_sampling_ratio_);

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