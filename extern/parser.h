#ifndef __HW1__PARSER__
#define __HW1__PARSER__

#include <ostream>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <iostream>
#include <fstream>
#include <string>
#include <algorithm>

#include "ply.h"

#define FP_PRECISION double

namespace parser {
// Notice that all the structures are as simple as possible
// so that you are not enforced to adopt any style or design.
enum RawMaterialType { kDefault, kMirror, kConductor, kDielectric };

enum RawTextureMapType { kImage, kPerlin, kCheckerboard };

enum RawTextureMapDecalMode {
  kReplaceKd,
  kBlendKd,
  kReplaceKs,
  kReplaceBackground,
  kReplaceNormal,
  kBumpNormal,
  kReplaceAll
};

enum RawTextureMapInterpolationMode { kNearest, kBilinear, kTrilinear };

enum RawTextureMapNoiseConversionType { kAbsVal, kLinear };

enum RawToneMappingAlgorithm { kPhotographic, kFilmic, kACES };

enum RawEnvironmentMapType { kProbe, kLatLong };

enum RawEnvironmentMapSampler { kUniform, kCosine };

struct Vec2f {
  FP_PRECISION x, y;
};

struct Vec3f {
  FP_PRECISION x, y, z;

  FP_PRECISION operator[](size_t index) {
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

  FP_PRECISION operator[](size_t index) {
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

  FP_PRECISION operator[](size_t index) {
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
  FP_PRECISION x, y, z, w;
};

struct Vec5f {
  FP_PRECISION x, y, z, w, t;
};

struct RawToneMapping {
  RawToneMappingAlgorithm algorithm;
  FP_PRECISION key = 0.18;
  FP_PRECISION burn = 0.0;
  FP_PRECISION saturation = 1.0;
  FP_PRECISION gamma = 1.0;
  std::string extension = "";
};

struct RawCamera {
  bool look_at_camera = false;
  Vec3f position;
  Vec3f gaze;
  Vec3f gaze_point;
  Vec3f up;
  FP_PRECISION fov_y;
  Vec4f near_plane;
  FP_PRECISION near_distance;
  FP_PRECISION focus_distance;
  FP_PRECISION aperture_size;
  unsigned int num_samples;
  int image_width, image_height;
  std::string image_name;
  std::string transformations = "";
  std::vector<RawToneMapping> tone_mappings = {};
};

struct RawPointLight {
  Vec3f position;
  Vec3f intensity;
  std::string transformations = "";
};

struct RawAreaLight {
  Vec3f position;
  Vec3f radiance;
  Vec3f normal;
  FP_PRECISION size;
  std::string transformations = "";
};

struct RawDirectionalLight {
  Vec3f direction;
  Vec3f radiance;
};

struct RawSpotLight {
  Vec3f position;
  Vec3f direction;
  Vec3f intensity;
  FP_PRECISION coverage_angle;
  FP_PRECISION falloff_angle;
};

struct RawSphericalDirectionalLight{
  bool exists = false;
  RawEnvironmentMapType type;
  RawEnvironmentMapSampler sampler;
  int image_id;
};

struct RawMaterial {
  RawMaterialType material_type;
  Vec3f ambient;
  Vec3f diffuse;
  Vec3f specular;
  Vec3f mirror;
  Vec3f absorption_coefficient;
  FP_PRECISION refraction_index;
  FP_PRECISION absorption_index;
  FP_PRECISION phong_exponent;
  FP_PRECISION roughness = 0.0;
};

struct RawFace {
  int v0_id;
  int v1_id;
  int v2_id;
};

struct RawMesh {
  int object_id;
  int material_id;
  std::string textures;
  std::vector<RawFace> faces;
  long long vertex_offset = 0;
  long long tex_coord_offset = 0;
  std::string ply_filepath = "";
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawMeshInstance {
  int object_id;
  int material_id;
  std::string textures;
  int base_object_id;
  bool reset_transform;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawTriangle {
  int object_id;
  int material_id;
  std::string textures;
  RawFace indices;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawSphere {
  int object_id;
  int material_id;
  std::string textures;
  int center_vertex_id;
  FP_PRECISION radius;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawPlane {
  int object_id;
  int material_id;
  std::string textures;
  int point_vertex_id;
  Vec3f normal;
  std::string transformations = "";
  Vec3f motion_blur = {0, 0, 0};
};

struct RawTranslation {
  FP_PRECISION tx, ty, tz;
};

struct RawScaling {
  FP_PRECISION sx, sy, sz;
};

struct RawScalingFlip {
  bool sx, sy, sz;
};

struct RawRotation {
  FP_PRECISION angle, x, y, z;
};

struct RawComposite {
  FP_PRECISION m[4][4];
};

struct RawImage {
  std::string path;
};

struct RawTextureMap {
  RawTextureMapType type;
  int image_id;
  RawTextureMapDecalMode decal_mode;
  RawTextureMapInterpolationMode interpolation_mode = RawTextureMapInterpolationMode::kNearest;
  FP_PRECISION normalizer;
  FP_PRECISION bump_factor;
  RawTextureMapNoiseConversionType noise_conversion;
  FP_PRECISION noise_scale;
  int num_octaves;
  FP_PRECISION scale;
  FP_PRECISION offset;
  Vec3f black_color;
  Vec3f white_color;
};

struct Mat4x4f {
  Mat4x4f() {}
  Mat4x4f(std::vector<std::vector<FP_PRECISION>> a) {
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


  FP_PRECISION m[4][4];

  FP_PRECISION operator[](size_t index) { return m[index / 4][index % 4]; }

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

struct Mat2x2f {
  Mat2x2f() {}
  Mat2x2f(std::vector<std::vector<FP_PRECISION>> a) {
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        m[i][j] = a[i][j];
      }
    }
  }
  Mat2x2f(RawComposite c) {
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        m[i][j] = c.m[i][j];
      }
    }
  }
  FP_PRECISION m[2][2];

  FP_PRECISION operator[](size_t index) { return m[index / 2][index % 2]; }
};

struct RawScene {
  ~RawScene() {
    cameras.clear();
    point_lights.clear();
    materials.clear();
    vertex_data.clear();
    tex_coord_data.clear();
    meshes.clear();
    triangles.clear();
    spheres.clear();
    planes.clear();
    images.clear();
    texture_maps.clear();
  }

  // Data
  Vec3i background_color;
  FP_PRECISION shadow_ray_epsilon;
  FP_PRECISION intersection_test_epsilon;
  int max_recursion_depth;
  std::vector<RawCamera> cameras;
  Vec3f ambient_light;
  std::vector<RawPointLight> point_lights;
  std::vector<RawAreaLight> area_lights;
  std::vector<RawDirectionalLight> directional_lights;
  std::vector<RawSpotLight> spot_lights;
  RawSphericalDirectionalLight spherical_directional_light;
  std::vector<RawMaterial> materials;
  std::vector<Vec3f> vertex_data;
  std::vector<Vec2f> tex_coord_data;
  std::vector<RawMesh> meshes;
  std::vector<RawMeshInstance> mesh_instances;
  std::vector<RawTriangle> triangles;
  std::vector<RawSphere> spheres;
  std::vector<RawPlane> planes;

  std::vector<RawTranslation> translations;
  std::vector<RawScaling> scalings;
  std::vector<RawRotation> rotations;
  std::vector<RawComposite> composites;

  std::vector<RawImage> images;
  std::vector<RawTextureMap> texture_maps;

  // Functions
  void loadFromXml(const std::string& filepath);
  void loadFromJSON(const std::string& filepath);
};
}  // namespace parser

#endif
