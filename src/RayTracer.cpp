#include <iostream>

#include "Scene.hpp"

int main(int argc, char *argv[]) {
  if (argc != 2) {
    std::cerr << "Usage: " << argv[0] << " <input_file>" << std::endl;
    return 1;
  }

  Scene scene(argv[1]);

  return 0;
}