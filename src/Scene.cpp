#include "Scene.hpp"

Scene::Scene(const std::string &filename) : filename_(filename) {
  LoadScene();
  PreprocessScene();
}

Scene::~Scene() {
  cameras_.clear();
  point_lights_.clear();
  ambient_lights_.clear();
  materials_.clear();
  objects_.clear();
}

void Scene::LoadScene() {
  RawScene raw_scene;
  raw_scene.loadFromXml(filename_);

  background_color_ = raw_scene.background_color;
  shadow_ray_epsilon_ = raw_scene.shadow_ray_epsilon;
  max_recursion_depth_ = raw_scene.max_recursion_depth;

  ambient_lights_.push_back(
      std::make_shared<AmbientLightSource>(raw_scene.ambient_light));

  for (const auto &raw_point_light : raw_scene.point_lights) {
    point_lights_.push_back(std::make_shared<PointLightSource>(
        raw_point_light.position, raw_point_light.intensity));
  }

  for (const auto &raw_camera : raw_scene.cameras) {
    cameras_.push_back(std::make_shared<BaseCamera>(
        raw_camera.position, raw_camera.gaze, raw_camera.up,
        raw_camera.near_plane, raw_camera.near_distance, raw_camera.image_width,
        raw_camera.image_height, raw_camera.image_name));
  }

  for (const auto &raw_material : raw_scene.materials) {
    switch (raw_material.material_type) {
      case RawMaterialType::kDefault:
        materials_.push_back(std::make_shared<BaseMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent));
        break;

      case RawMaterialType::kMirror:
        materials_.push_back(std::make_shared<MirrorMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.mirror));
        break;
      case RawMaterialType::kConductor:
        materials_.push_back(std::make_shared<ConductorMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.mirror,
            raw_material.refraction_index, raw_material.absorption_index));
        break;
      case RawMaterialType::kDielectric:
        materials_.push_back(std::make_shared<DielectricMaterial>(
            raw_material.ambient, raw_material.diffuse, raw_material.specular,
            raw_material.phong_exponent, raw_material.mirror,
            raw_material.absorption_coefficient,
            raw_material.refraction_index));
        break;
    }
  }

  for (const auto &raw_sphere : raw_scene.spheres) {
    objects_.push_back(std::make_shared<SphereObject>(
        materials_[raw_sphere.material_id],
        raw_scene.vertex_data[raw_sphere.center_vertex_id], raw_sphere.radius));
  }

  for (const auto &raw_triangle : raw_scene.triangles) {
    objects_.push_back(std::make_shared<TriangleObject>(
        materials_[raw_triangle.material_id],
        raw_scene.vertex_data[raw_triangle.indices.v0_id],
        raw_scene.vertex_data[raw_triangle.indices.v1_id],
        raw_scene.vertex_data[raw_triangle.indices.v2_id]));
  }

  for (const auto &raw_mesh : raw_scene.meshes) {
    auto mesh_object =
        std::make_shared<MeshObject>(materials_[raw_mesh.material_id]);
    std::unordered_map<int, int> face_id_map;
    for (const auto &raw_face : raw_mesh.faces) {
      int v0_id = raw_face.v0_id;
      int v1_id = raw_face.v1_id;
      int v2_id = raw_face.v2_id;

      if (face_id_map.find(v0_id) == face_id_map.end()) {
        face_id_map[v0_id] = mesh_object->vertex_data_.size();
        mesh_object->vertex_data_.push_back(raw_scene.vertex_data[v0_id]);
      }
      if (face_id_map.find(v1_id) == face_id_map.end()) {
        face_id_map[v1_id] = mesh_object->vertex_data_.size();
        mesh_object->vertex_data_.push_back(raw_scene.vertex_data[v1_id]);
      }
      if (face_id_map.find(v2_id) == face_id_map.end()) {
        face_id_map[v2_id] = mesh_object->vertex_data_.size();
        mesh_object->vertex_data_.push_back(raw_scene.vertex_data[v2_id]);
      }
      mesh_object->face_data_.push_back(
          {face_id_map[v0_id], face_id_map[v1_id], face_id_map[v2_id]});
    }
    objects_.push_back(mesh_object);
  }
}

void Scene::PreprocessScene() {
  for (const auto &object : objects_) {
    object->Preprocess();
  }
}