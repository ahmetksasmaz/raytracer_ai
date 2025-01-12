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

struct Vec2f {
  float x, y;
};

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

  bool operator==(const Vec2i& other) const {
    return x == other.x && y == other.y;
  }

  friend std::ostream& operator<<(std::ostream& os, const Vec2i& vec) {
    os << "(" << vec.x << ", " << vec.y << ")";
    return os;
  }
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

struct Vec5f {
  float x, y, z, w, t;
};

struct RawSpectrum {
  int min_wavelength;
  int max_wavelength;
  std::vector<float> samples;
};

enum struct SensorSize {
  kFullFrame,
  kAPSH,
  kAPSC,
  kFourThirds,
  kOneInch,
  kTwoOverThree,
};

enum struct Aperture {
  kF1_0,
  kF1_4,
  kF2_0,
  kF2_8,
  kF4_0,
  kF5_6,
  kF8_0,
  kF11_0,
  kF16_0,
  kF22_0,
  kF32_0,
};

enum struct ExposureTime {
  k1_8000,
  k1_4000,
  k1_2000,
  k1_1000,
  k1_500,
  k1_250,
  k1_125,
  k1_60,
  k1_30,
  k1_15,
  k1_8,
  k1_4,
  k1_2,
  k1,
  k2,
  k4,
  k8,
  k15,
  k30,
};

enum struct ISO {
  k100,
  k200,
  k400,
  k800,
  k1600,
  k3200,
  k6400,
  k12800,
  k25600,
  k51200,
  k102400,
};

struct RawCamera {
  Vec3f position;
  Vec3f gaze;
  Vec3f up;
  SensorSize sensor_size;
  Aperture aperture;
  ExposureTime exposure_time;
  ISO iso;
  int pixel_size;
  int focal_length;
  Vec2i sensor_pattern;
  std::vector<int> color_filter_array_spectrums;
  int quantum_efficiency_spectrum;
  float full_well_capacity;
  int quantization_level;

  std::string image_name;
};

struct RawAmbientLight {
  float power;
  int spectrum_id;
};

struct RawPointLight {
  Vec3f position;
  float power;
  int spectrum_id;
};

struct RawAreaLight {
  Vec3f position;
  Vec3f normal;
  float size;
  float power;
  int spectrum_id;
};

struct RawMaterial {
  RawMaterialType material_type;
  int ambient_spectrum_id;
  int diffuse_spectrum_id;
  int specular_spectrum_id;
  int mirror_spectrum_id;
  int absorption_coefficient_spectrum_id;
  float refraction_index;
  float absorption_index;
  float phong_exponent;
  float roughness = 0.0;
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
  Vec3f motion_blur = {0, 0, 0};
};

struct RawMeshInstance {
  int object_id;
  int material_id;
  int base_object_id;
  bool reset_transform;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawTriangle {
  int object_id;
  int material_id;
  RawFace indices;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawSphere {
  int object_id;
  int material_id;
  int center_vertex_id;
  float radius;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawTranslation {
  float tx, ty, tz;
};

struct RawScaling {
  float sx, sy, sz;
};

struct RawScalingFlip {
  bool sx, sy, sz;
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

  float operator[](size_t index) { return m[index / 4][index % 4]; }

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
  std::vector<RawSpectrum> spectrums;
  std::vector<RawCamera> cameras;
  RawAmbientLight ambient_light;
  std::vector<RawPointLight> point_lights;
  std::vector<RawAreaLight> area_lights;
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
