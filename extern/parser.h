#ifndef __HW1__PARSER__
#define __HW1__PARSER__

#include <ostream>
#include <string>
#include <vector>

#include "ply.h"

namespace parser {
// Notice that all the structures are as simple as possible
// so that you are not enforced to adopt any style or design.
enum RawMaterialType { kDefault, kMirror, kConductor, kDielectric };

struct Vec3f {
  float x, y, z;

  float operator[](size_t index) {
    switch (index) {
      case 0:
        return x;
      case 1:
        return y;
      case 2:
        return z;
      default:
        return 0;
    }
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec3f& vec) {
    os << "(" << vec.x << ", " << vec.y << ", " << vec.z << ")";
    return os;
  }
};

struct Vec2i {
  int x, y;
};

struct Vec3i {
  int x, y, z;

  float operator[](size_t index) {
    switch (index) {
      case 0:
        return x;
      case 1:
        return y;
      case 2:
        return z;
      default:
        return 0;
    }
  }
};

struct Vec3uc {
  unsigned char r, g, b;

  float operator[](size_t index) {
    switch (index) {
      case 0:
        return r;
      case 1:
        return g;
      case 2:
        return b;
      default:
        return 0;
    }
  }
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
  int object_id;
  int material_id;
  std::vector<RawFace> faces;
  std::string ply_filepath = "";
  std::string transformations = "";
};

struct RawMeshInstance {
  int object_id;
  int material_id;
  int base_object_id;
  bool reset_transform;
  std::string transformations = "";
};

struct RawTriangle {
  int object_id;
  int material_id;
  RawFace indices;
  std::string transformations = "";
};

struct RawSphere {
  int object_id;
  int material_id;
  int center_vertex_id;
  float radius;
  std::string transformations = "";
};

struct RawTranslation {
  float tx, ty, tz;
};

struct RawScaling {
  float sx, sy, sz;
};

struct RawRotation {
  float angle, x, y, z;
};

struct RawComposite {
  float m[4][4];
};

struct Mat4x4f {
  Mat4x4f() {}
  Mat4x4f(std::vector<std::vector<float>> a) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        m[i][j] = a[i][j];
      }
    }
  }
  Mat4x4f(RawComposite c) {
    for (int i = 0; i < 4; i++) {
      for (int j = 0; j < 4; j++) {
        m[i][j] = c.m[i][j];
      }
    }
  }

  float m[4][4];

  friend std::ostream& operator<<(std::ostream& os, const Mat4x4f& matrix) {
    for (int i = 0; i < 4; i++) {
      os << " | ";
      for (int j = 0; j < 4; j++) {
        os << matrix.m[i][j] << " | ";
      }
      os << std::endl;
    }

    return os;
  }
};

struct RawScene {
  ~RawScene() {
    cameras.clear();
    point_lights.clear();
    materials.clear();
    vertex_data.clear();
    meshes.clear();
    triangles.clear();
    spheres.clear();
  }

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
  std::vector<RawMeshInstance> mesh_instances;
  std::vector<RawTriangle> triangles;
  std::vector<RawSphere> spheres;

  std::vector<RawTranslation> translations;
  std::vector<RawScaling> scalings;
  std::vector<RawRotation> rotations;
  std::vector<RawComposite> composites;

  // Functions
  void loadFromXml(const std::string& filepath);
};
}  // namespace parser

#endif
