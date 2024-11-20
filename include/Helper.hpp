#pragma once

// #define DEBUG

#include <math.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "../extern/parser.h"

using namespace parser;

const Mat4x4f IDENTITY_MATRIX = {
    {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};

inline Vec3f cross(Vec3f a, Vec3f b) {
  return Vec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x};
}

inline Vec3f normalize(Vec3f a) {
  float norm = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
  return Vec3f{a.x / norm, a.y / norm, a.z / norm};
}

inline float norm2(Vec3f a) { return a.x * a.x + a.y * a.y + a.z * a.z; }

inline float norm(Vec3f a) { return sqrt(norm2(a)); }

inline float norm(Vec2f a) { return sqrt(a.x * a.x + a.y * a.y); }

inline float dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3f hadamard(Vec3f a, Vec3f b) {
  return Vec3f{a.x * b.x, a.y * b.y, a.z * b.z};
}

inline Vec3f operator*(Vec3f a, float b) {
  return Vec3f{a.x * b, a.y * b, a.z * b};
}

inline Vec3f operator*(float a, Vec3f b) {
  return Vec3f{a * b.x, a * b.y, a * b.z};
};

inline Vec3f operator/(Vec3f a, float b) {
  return Vec3f{a.x / b, a.y / b, a.z / b};
}

inline Vec3f operator+(Vec3f a, Vec3f b) {
  return Vec3f{a.x + b.x, a.y + b.y, a.z + b.z};
}

inline Vec2f operator+(Vec2f a, Vec2f b) { return Vec2f{a.x + b.x, a.y + b.y}; }

inline Vec3f operator-(Vec3f a) { return Vec3f{-a.x, -a.y, -a.z}; }

inline Vec2f operator-(Vec2f a, Vec2f b) { return Vec2f{a.x - b.x, a.y - b.y}; }

inline Vec3f operator+=(Vec3f& a, Vec3f b) {
  a.x += b.x;
  a.y += b.y;
  a.z += b.z;
  return a;
}

inline Vec3f operator-=(Vec3f& a, Vec3f b) {
  a.x -= b.x;
  a.y -= b.y;
  a.z -= b.z;
  return a;
}

inline Vec3f operator-(Vec3f a, Vec3f b) {
  return Vec3f{a.x - b.x, a.y - b.y, a.z - b.z};
}

inline Vec3f bounding_volume_min(std::vector<Vec3f> vertices) {
  return Vec3f{
      std::min_element(vertices.begin(), vertices.end(),
                       [](Vec3f a, Vec3f b) { return a.x < b.x; })
          ->x,
      std::min_element(vertices.begin(), vertices.end(),
                       [](Vec3f a, Vec3f b) { return a.y < b.y; })
          ->y,
      std::min_element(vertices.begin(), vertices.end(), [](Vec3f a, Vec3f b) {
        return a.z < b.z;
      })->z};
}

inline Vec3f bounding_volume_max(std::vector<Vec3f> vertices) {
  return Vec3f{
      std::max_element(vertices.begin(), vertices.end(),
                       [](Vec3f a, Vec3f b) { return a.x < b.x; })
          ->x,
      std::max_element(vertices.begin(), vertices.end(),
                       [](Vec3f a, Vec3f b) { return a.y < b.y; })
          ->y,
      std::max_element(vertices.begin(), vertices.end(), [](Vec3f a, Vec3f b) {
        return a.z < b.z;
      })->z};
}

inline Mat4x4f operator*(Mat4x4f a, Mat4x4f b) {
  Mat4x4f result;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      result.m[i][j] = 0;
      for (int k = 0; k < 4; k++) {
        result.m[i][j] += a.m[i][k] * b.m[k][j];
      }
    }
  }
  return result;
}

inline Mat4x4f operator*(Mat4x4f a, float b) {
  Mat4x4f result;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      result.m[i][j] = a.m[i][j] * b;
    }
  }
  return result;
}

inline Mat4x4f operator!(Mat4x4f a) {
  Mat4x4f result;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      result.m[i][j] = a.m[j][i];
    }
  }
  return result;
}

inline Mat4x4f operator~(Mat4x4f a) {
  Mat4x4f result;
  result.m[0][0] = a[5] * a[10] * a[15] - a[5] * a[11] * a[14] -
                   a[9] * a[6] * a[15] + a[9] * a[7] * a[14] +
                   a[13] * a[6] * a[11] - a[13] * a[7] * a[10];

  result.m[1][0] = -a[4] * a[10] * a[15] + a[4] * a[11] * a[14] +
                   a[8] * a[6] * a[15] - a[8] * a[7] * a[14] -
                   a[12] * a[6] * a[11] + a[12] * a[7] * a[10];

  result.m[2][0] = a[4] * a[9] * a[15] - a[4] * a[11] * a[13] -
                   a[8] * a[5] * a[15] + a[8] * a[7] * a[13] +
                   a[12] * a[5] * a[11] - a[12] * a[7] * a[9];

  result.m[3][0] = -a[4] * a[9] * a[14] + a[4] * a[10] * a[13] +
                   a[8] * a[5] * a[14] - a[8] * a[6] * a[13] -
                   a[12] * a[5] * a[10] + a[12] * a[6] * a[9];

  result.m[0][1] = -a[1] * a[10] * a[15] + a[1] * a[11] * a[14] +
                   a[9] * a[2] * a[15] - a[9] * a[3] * a[14] -
                   a[13] * a[2] * a[11] + a[13] * a[3] * a[10];

  result.m[1][1] = a[0] * a[10] * a[15] - a[0] * a[11] * a[14] -
                   a[8] * a[2] * a[15] + a[8] * a[3] * a[14] +
                   a[12] * a[2] * a[11] - a[12] * a[3] * a[10];

  result.m[2][1] = -a[0] * a[9] * a[15] + a[0] * a[11] * a[13] +
                   a[8] * a[1] * a[15] - a[8] * a[3] * a[13] -
                   a[12] * a[1] * a[11] + a[12] * a[3] * a[9];

  result.m[3][1] = a[0] * a[9] * a[14] - a[0] * a[10] * a[13] -
                   a[8] * a[1] * a[14] + a[8] * a[2] * a[13] +
                   a[12] * a[1] * a[10] - a[12] * a[2] * a[9];

  result.m[0][2] = a[1] * a[6] * a[15] - a[1] * a[7] * a[14] -
                   a[5] * a[2] * a[15] + a[5] * a[3] * a[14] +
                   a[13] * a[2] * a[7] - a[13] * a[3] * a[6];

  result.m[1][2] = -a[0] * a[6] * a[15] + a[0] * a[7] * a[14] +
                   a[4] * a[2] * a[15] - a[4] * a[3] * a[14] -
                   a[12] * a[2] * a[7] + a[12] * a[3] * a[6];

  result.m[2][2] = a[0] * a[5] * a[15] - a[0] * a[7] * a[13] -
                   a[4] * a[1] * a[15] + a[4] * a[3] * a[13] +
                   a[12] * a[1] * a[7] - a[12] * a[3] * a[5];

  result.m[3][2] = -a[0] * a[5] * a[14] + a[0] * a[6] * a[13] +
                   a[4] * a[1] * a[14] - a[4] * a[2] * a[13] -
                   a[12] * a[1] * a[6] + a[12] * a[2] * a[5];

  result.m[0][3] = -a[1] * a[6] * a[11] + a[1] * a[7] * a[10] +
                   a[5] * a[2] * a[11] - a[5] * a[3] * a[10] -
                   a[9] * a[2] * a[7] + a[9] * a[3] * a[6];

  result.m[1][3] = a[0] * a[6] * a[11] - a[0] * a[7] * a[10] -
                   a[4] * a[2] * a[11] + a[4] * a[3] * a[10] +
                   a[8] * a[2] * a[7] - a[8] * a[3] * a[6];

  result.m[2][3] = -a[0] * a[5] * a[11] + a[0] * a[7] * a[9] +
                   a[4] * a[1] * a[11] - a[4] * a[3] * a[9] -
                   a[8] * a[1] * a[7] + a[8] * a[3] * a[5];

  result.m[3][3] = a[0] * a[5] * a[10] - a[0] * a[6] * a[9] -
                   a[4] * a[1] * a[10] + a[4] * a[2] * a[9] +
                   a[8] * a[1] * a[6] - a[8] * a[2] * a[5];

  float det = a[0] * result[0] + a[1] * result[4] + a[2] * result[8] +
              a[3] * result[12];

  det = 1.0 / det;

  result = result * det;

  return result;
}

inline Vec3f operator*(Mat4x4f a, Vec3f b) {
  return Vec3f{a.m[0][0] * b.x + a.m[0][1] * b.y + a.m[0][2] * b.z + a.m[0][3],
               a.m[1][0] * b.x + a.m[1][1] * b.y + a.m[1][2] * b.z + a.m[1][3],
               a.m[2][0] * b.x + a.m[2][1] * b.y + a.m[2][2] * b.z + a.m[2][3]};
}

inline Mat4x4f translation_matrix(RawTranslation t) {
  return Mat4x4f{
      {{1, 0, 0, t.tx}, {0, 1, 0, t.ty}, {0, 0, 1, t.tz}, {0, 0, 0, 1}}};
}

inline Mat4x4f scaling_matrix(RawScaling s) {
  return Mat4x4f{
      {{s.sx, 0, 0, 0}, {0, s.sy, 0, 0}, {0, 0, s.sz, 0}, {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix_x(float angle_degree) {
  float angle = angle_degree * M_PI / 180;
  return Mat4x4f{{{1, 0, 0, 0},
                  {0, cos(angle), -sin(angle), 0},
                  {0, sin(angle), cos(angle), 0},
                  {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix_y(float angle_degree) {
  float angle = angle_degree * M_PI / 180;
  return Mat4x4f{{{cos(angle), 0, sin(angle), 0},
                  {0, 1, 0, 0},
                  {-sin(angle), 0, cos(angle), 0},
                  {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix_z(float angle_degree) {
  float angle = angle_degree * M_PI / 180;
  return Mat4x4f{{{cos(angle), -sin(angle), 0, 0},
                  {sin(angle), cos(angle), 0, 0},
                  {0, 0, 1, 0},
                  {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix(RawRotation r) {
  Mat4x4f result = IDENTITY_MATRIX;
  if (r.x != 0) {
    result = rotation_matrix_x(r.angle) * result;
  }
  if (r.y != 0) {
    result = rotation_matrix_y(r.angle) * result;
  }
  if (r.z != 0) {
    result = rotation_matrix_z(r.angle) * result;
  }
  return result;
}

inline Mat4x4f parse_transformation(std::string transformation_text,
                                    RawScalingFlip& scaling_flip,
                                    std::vector<RawTranslation>& translations,
                                    std::vector<RawScaling>& scalings,
                                    std::vector<RawRotation>& rotations,
                                    std::vector<RawComposite>& composites) {
  Mat4x4f result = IDENTITY_MATRIX;
  std::stringstream ss(transformation_text);
  std::string transformation;
  while (getline(ss, transformation, ' ')) {
    Mat4x4f multiplier_matrix;

    if (transformation[0] == 't') {
      multiplier_matrix = translation_matrix(
          translations[std::stoi(transformation.substr(1)) - 1]);
    } else if (transformation[0] == 's') {
      auto scaling = scalings[std::stoi(transformation.substr(1)) - 1];
      multiplier_matrix = scaling_matrix(scaling);
      if (scaling.sx < 0) {
        scaling_flip.sx = !scaling_flip.sx;
      }
      if (scaling.sy < 0) {
        scaling_flip.sy = !scaling_flip.sy;
      }
      if (scaling.sz < 0) {
        scaling_flip.sz = !scaling_flip.sz;
      }
    } else if (transformation[0] == 'r') {
      multiplier_matrix =
          rotation_matrix(rotations[std::stoi(transformation.substr(1)) - 1]);
    } else if (transformation[0] == 'c') {
      multiplier_matrix = composites[std::stoi(transformation.substr(1)) - 1];
    }
    result = multiplier_matrix * result;
  }
  return result;
}

template <typename T>
inline void shuffle(std::vector<T>& samples) {
  for (int i = 0; i < samples.size(); i++) {
    int j = rand() % samples.size();
    std::swap(samples[i], samples[j]);
  }
}

inline std::vector<float> uniform_1d(int num_samples) {
  std::vector<float> samples;
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((float)i / num_samples);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> uniform_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    for (int j = 0; j < num_samples; j++) {
      samples.push_back(Vec2f{(float)i / num_samples, (float)j / num_samples});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<float> uniform_random_1d(int num_samples) {
  std::vector<float> samples;
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((float)rand() / RAND_MAX);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> uniform_random_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    for (int j = 0; j < num_samples; j++) {
      samples.push_back(
          Vec2f{(float)rand() / RAND_MAX, (float)rand() / RAND_MAX});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<float> jittered_1d(int num_samples) {
  std::vector<float> samples;
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((i + (float)rand() / RAND_MAX) / num_samples);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> jittered_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    for (int j = 0; j < num_samples; j++) {
      samples.push_back(Vec2f{(i + (float)rand() / RAND_MAX) / num_samples,
                              (j + (float)rand() / RAND_MAX) / num_samples});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> multi_jittered_2d(int num_samples) {
  std::vector<Vec2f> samples;
  int n = sqrt(num_samples);
  float subcell_width = 1.0 / num_samples;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      samples.push_back(
          Vec2f{(i * n + j + (float)rand() / RAND_MAX) * subcell_width,
                (j * n + i + (float)rand() / RAND_MAX) * subcell_width});
    }
  }
  shuffle(samples);
  return samples;
}

inline float radical_inverse(unsigned int n, unsigned int base) {
  float val = 0;
  float inv_base = 1.0 / base;
  float inv_bi = inv_base;
  while (n > 0) {
    unsigned int d_i = n % base;
    val += d_i * inv_bi;
    n /= base;
    inv_bi *= inv_base;
  }
  return val;
}

inline std::vector<Vec2f> hammersley_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    float x = (float)i / num_samples;
    float y = (float)radical_inverse(i, 2);
    samples.push_back(Vec2f{x, y});
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> halton_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    float x = (float)radical_inverse(i, 2);
    float y = (float)radical_inverse(i, 3);
    samples.push_back(Vec2f{x, y});
  }
  shuffle(samples);
  return samples;
}