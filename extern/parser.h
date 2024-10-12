#ifndef __HW1__PARSER__
#define __HW1__PARSER__

#include <string>
#include <vector>

namespace parser {
// Notice that all the structures are as simple as possible
// so that you are not enforced to adopt any style or design.
enum RawMaterialType { kMirror, kConductor, kDielectric };

struct Vec3f {
  float x, y, z;
};

struct Vec2i {
  int x, y;
};

struct Vec3i {
  int x, y, z;
};

struct Vec4f {
  float x, y, z, w;
};

struct RawCamera {
  Vec3f position;
  Vec3f gaze;
  Vec3f up;
  Vec4f near_plane;
  float near_distance;
  int image_width, image_height;
  std::string image_name;
};

struct RawPointLight {
  Vec3f position;
  Vec3f intensity;
};

struct RawMaterial {
  RawMaterialType material_type;
  Vec3f ambient;
  Vec3f diffuse;
  Vec3f specular;
  Vec3f mirror;
  Vec3f absorption_coefficient;
  float refraction_index;
  float absorption_index;
  float phong_exponent;
};

struct RawFace {
  int v0_id;
  int v1_id;
  int v2_id;
};

struct RawMesh {
  int material_id;
  std::vector<RawFace> faces;
};

struct RawTriangle {
  int material_id;
  RawFace indices;
};

struct RawSphere {
  int material_id;
  int center_vertex_id;
  float radius;
};

struct RawScene {
  // Data
  Vec3i background_color;
  float shadow_ray_epsilon;
  int max_recursion_depth;
  std::vector<RawCamera> cameras;
  Vec3f ambient_light;
  std::vector<RawPointLight> point_lights;
  std::vector<RawMaterial> materials;
  std::vector<Vec3f> vertex_data;
  std::vector<RawMesh> meshes;
  std::vector<RawTriangle> triangles;
  std::vector<RawSphere> spheres;

  // Functions
  void loadFromXml(const std::string &filepath);
};
}  // namespace parser

#endif
