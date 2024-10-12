#pragma once
#include <iostream>
#include <memory>

#include "../extern/parser.h"
#include "BaseCamera.hpp"
#include "BaseMaterial.hpp"
#include "BaseObject.hpp"
#include "BasePointLight.hpp"

using namespace parser;

class Scene {
 public:
  Scene(std::string filename);
  void Render();
  ~Scene();

 private:
  void LoadScene();
  void PreprocessScene();

  std::string filename_;

  RawScene raw_scene_;
  Vec3i background_color_;
  float shadow_ray_epsilon_;
  int max_recursion_depth_;
  Vec3f ambient_light_;

  std::vector<std::shared_ptr<BaseCamera>> cameras_;
  std::vector<std::shared_ptr<BasePointLight>> point_lights_;
  std::vector<std::shared_ptr<BaseMaterial>> materials_;
  std::vector<std::shared_ptr<BaseObject>> objects_;
};