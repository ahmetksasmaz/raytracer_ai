#include <iostream>

#include "Scene.hpp"
#include "Timer.hpp"

Timer timer;

int main(int argc, char *argv[]) {
  if (argc < 2) {
    std::cerr << "Usage: " << argv[0] << " <input_file>"
              << " <configuration_file [OPTIONAL]>" << std::endl;
    return 1;
  }

  Configuration configuration;

  if (argc == 3) {
    configuration.ParseFromFile(argv[2]);
  }

  timer.configuration_ = configuration;

  Scene scene(argv[1], configuration);

  scene.Render();

  timer.AnalyzeTimeLogs();

  return 0;
}