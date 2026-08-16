#include <cstring>
#include <iostream>

#include "Scene.hpp"
#include "Timer.hpp"

Timer timer;

int main(int argc, char *argv[]) {
  const char *scene_file = nullptr;
  bool serial = false;

  for (int i = 1; i < argc; i++) {
    if (std::strcmp(argv[i], "--serial") == 0) {
      serial = true;
    } else if (!scene_file) {
      scene_file = argv[i];
    }
  }

  if (!scene_file) {
    std::cerr << "Usage: " << argv[0] << " <scene.json> [--serial]" << std::endl;
    std::cerr << "  --serial  render on one thread; use to tell a race in the"
                 " trace path from a bug in the physics" << std::endl;
    return 1;
  }

  Scene scene(scene_file, serial);

  scene.Render();

  timer.AnalyzeTimeLogs();

  return 0;
}
