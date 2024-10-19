#include <iostream>

#include "Scene.hpp"

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input_file>"
              << " <ray_tracing_algorithm [OPTIONAL]>"
              << " <scheduling_algorithm [OPTIONAL]>"
              << " <tonemapping_algorithm [OPTIONAL]>"
              << " <exporter_type [OPTIONAL]>" << std::endl;
    return 1;
  }

  RayTracingAlgorithm ray_tracing_algorithm = RayTracingAlgorithm::kBest;
  SchedulingAlgorithm scheduling_algorithm = SchedulingAlgorithm::kBest;
  ToneMappingAlgorithm tonemapping_algorithm = ToneMappingAlgorithm::kBest;
  ExporterType exporter_type = ExporterType::kBest;

  if (argc >= 3) {
    int index = std::stoi(argv[2]);
    if (index > (int)RayTracingAlgorithm::kMax) {
      std::cerr << "Invalid ray tracing algorithm index." << std::endl;
      return 1;
    }
    ray_tracing_algorithm = static_cast<RayTracingAlgorithm>(index);
  }

  if (argc >= 4) {
    int index = std::stoi(argv[3]);
    if (index > (int)SchedulingAlgorithm::kMax) {
      std::cerr << "Invalid scheduling algorithm index." << std::endl;
      return 1;
    }
    scheduling_algorithm = static_cast<SchedulingAlgorithm>(index);
  }

  if (argc >= 5) {
    int index = std::stoi(argv[4]);
    if (index > (int)ToneMappingAlgorithm::kMax) {
      std::cerr << "Invalid tone mapping algorithm index." << std::endl;
      return 1;
    }
    tonemapping_algorithm = static_cast<ToneMappingAlgorithm>(index);
  }

  if (argc >= 6) {
    int index = std::stoi(argv[5]);
    if (index > (int)ExporterType::kMax) {
      std::cerr << "Invalid exporter type index." << std::endl;
      return 1;
    }
    exporter_type = static_cast<ExporterType>(index);
  }

  Scene scene(argv[1], ray_tracing_algorithm, scheduling_algorithm,
              tonemapping_algorithm, exporter_type);

  scene.Render();

  return 0;
}