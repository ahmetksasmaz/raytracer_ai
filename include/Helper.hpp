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
  FP_PRECISION norm = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
  return Vec3f{a.x / norm, a.y / norm, a.z / norm};
}

inline FP_PRECISION norm2(Vec3f a) { return a.x * a.x + a.y * a.y + a.z * a.z; }

inline FP_PRECISION norm(Vec3f a) { return sqrt(norm2(a)); }

inline FP_PRECISION norm(Vec2f a) { return sqrt(a.x * a.x + a.y * a.y); }

inline FP_PRECISION dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3f hadamard(Vec3f a, Vec3f b) {
  return Vec3f{a.x * b.x, a.y * b.y, a.z * b.z};
}

inline Vec3f operator*(Vec3f a, FP_PRECISION b) {
  return Vec3f{a.x * b, a.y * b, a.z * b};
}

inline Vec3f operator*(FP_PRECISION a, Vec3f b) {
  return Vec3f{a * b.x, a * b.y, a * b.z};
};

inline Vec3f operator/(Vec3f a, FP_PRECISION b) {
  return Vec3f{a.x / b, a.y / b, a.z / b};
}

inline Vec2f operator/(Vec2f a, FP_PRECISION b) { return Vec2f{a.x / b, a.y / b}; }

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

inline Mat4x4f operator*(Mat4x4f a, FP_PRECISION b) {
  Mat4x4f result;
  for (int i = 0; i < 4; i++) {
    for (int j = 0; j < 4; j++) {
      result.m[i][j] = a.m[i][j] * b;
    }
  }
  return result;
}

inline Mat2x2f operator*(Mat2x2f a, FP_PRECISION b) {
  Mat2x2f result;
  for (int i = 0; i < 2; i++) {
    for (int j = 0; j < 2; j++) {
      result.m[i][j] = a.m[i][j] * b;
    }
  }
  return result;
}

inline Vec2f operator*(Mat2x2f a, Vec2f b) {
  Vec2f result;
  result.x = a.m[0][0] * b.x + a.m[0][1] * b.y;
  result.y = a.m[1][0] * b.x + a.m[1][1] * b.y;
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

  FP_PRECISION det = a[0] * result[0] + a[1] * result[4] + a[2] * result[8] +
              a[3] * result[12];

  det = 1.0 / det;

  result = result * det;

  return result;
}

inline Mat2x2f operator~(Mat2x2f a) {
  Mat2x2f result;
  result.m[0][0] = a.m[1][1];
  result.m[0][1] = -a.m[0][1];
  result.m[1][0] = -a.m[1][0];
  result.m[1][1] = a.m[0][0];

  FP_PRECISION det = a.m[0][0] * a.m[1][1] - a.m[0][1] * a.m[1][0];
  det = 1.0 / det;

  result = result * det;

  return result;
}

inline Vec3f operator*(Mat4x4f a, Vec3f b) {
  return Vec3f{a.m[0][0] * b.x + a.m[0][1] * b.y + a.m[0][2] * b.z + a.m[0][3],
               a.m[1][0] * b.x + a.m[1][1] * b.y + a.m[1][2] * b.z + a.m[1][3],
               a.m[2][0] * b.x + a.m[2][1] * b.y + a.m[2][2] * b.z + a.m[2][3]};
}

inline Vec3f operator^(Mat4x4f a, Vec3f b) {
  return Vec3f{a.m[0][0] * b.x + a.m[0][1] * b.y + a.m[0][2] * b.z,
               a.m[1][0] * b.x + a.m[1][1] * b.y + a.m[1][2] * b.z,
               a.m[2][0] * b.x + a.m[2][1] * b.y + a.m[2][2] * b.z};
}

inline Mat4x4f translation_matrix(RawTranslation t) {
  return Mat4x4f{
      {{1, 0, 0, t.tx}, {0, 1, 0, t.ty}, {0, 0, 1, t.tz}, {0, 0, 0, 1}}};
}

inline Mat4x4f scaling_matrix(RawScaling s) {
  return Mat4x4f{
      {{s.sx, 0, 0, 0}, {0, s.sy, 0, 0}, {0, 0, s.sz, 0}, {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix_x(FP_PRECISION angle_degree) {
  FP_PRECISION angle = angle_degree * M_PI / 180;
  return Mat4x4f{{{1, 0, 0, 0},
                  {0, cos(angle), -sin(angle), 0},
                  {0, sin(angle), cos(angle), 0},
                  {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix_y(FP_PRECISION angle_degree) {
  FP_PRECISION angle = angle_degree * M_PI / 180;
  return Mat4x4f{{{cos(angle), 0, sin(angle), 0},
                  {0, 1, 0, 0},
                  {-sin(angle), 0, cos(angle), 0},
                  {0, 0, 0, 1}}};
}

inline Mat4x4f rotation_matrix_z(FP_PRECISION angle_degree) {
  FP_PRECISION angle = angle_degree * M_PI / 180;
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
  // Remove beginning and ending whitespaces
  transformation_text.erase(0, transformation_text.find_first_not_of(" \n\r\t"));
  transformation_text.erase(transformation_text.find_last_not_of(" \n\r\t") + 1);
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

inline std::vector<FP_PRECISION> uniform_1d(int num_samples) {
  std::vector<FP_PRECISION> samples;
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((FP_PRECISION)i / num_samples);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> uniform_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    for (int j = 0; j < num_samples; j++) {
      samples.push_back(Vec2f{(FP_PRECISION)i / num_samples, (FP_PRECISION)j / num_samples});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<FP_PRECISION> uniform_random_1d(int num_samples) {
  std::vector<FP_PRECISION> samples;
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((FP_PRECISION)rand() / RAND_MAX);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> uniform_random_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    for (int j = 0; j < num_samples; j++) {
      samples.push_back(
          Vec2f{(FP_PRECISION)rand() / RAND_MAX, (FP_PRECISION)rand() / RAND_MAX});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<FP_PRECISION> jittered_1d(int num_samples) {
  std::vector<FP_PRECISION> samples;
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((i + (FP_PRECISION)rand() / RAND_MAX) / num_samples);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> jittered_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    for (int j = 0; j < num_samples; j++) {
      samples.push_back(Vec2f{(i + (FP_PRECISION)rand() / RAND_MAX) / num_samples,
                              (j + (FP_PRECISION)rand() / RAND_MAX) / num_samples});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> multi_jittered_2d(int num_samples) {
  std::vector<Vec2f> samples;
  int n = sqrt(num_samples);
  FP_PRECISION subcell_width = 1.0 / num_samples;
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      samples.push_back(
          Vec2f{(i * n + j + (FP_PRECISION)rand() / RAND_MAX) * subcell_width,
                (j * n + i + (FP_PRECISION)rand() / RAND_MAX) * subcell_width});
    }
  }
  shuffle(samples);
  return samples;
}

inline FP_PRECISION radical_inverse(unsigned int n, unsigned int base) {
  FP_PRECISION val = 0;
  FP_PRECISION inv_base = 1.0 / base;
  FP_PRECISION inv_bi = inv_base;
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
    FP_PRECISION x = (FP_PRECISION)i / num_samples;
    FP_PRECISION y = (FP_PRECISION)radical_inverse(i, 2);
    samples.push_back(Vec2f{x, y});
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> halton_2d(int num_samples) {
  std::vector<Vec2f> samples;
  for (int i = 0; i < num_samples; i++) {
    FP_PRECISION x = (FP_PRECISION)radical_inverse(i, 2);
    FP_PRECISION y = (FP_PRECISION)radical_inverse(i, 3);
    samples.push_back(Vec2f{x, y});
  }
  shuffle(samples);
  return samples;
}

inline FP_PRECISION gaussian_kernel_weight(Vec2f diff, FP_PRECISION sigma) {
  // Center the coordinates (assuming the Gaussian kernel is centered at (0.5,
  // 0.5))
  FP_PRECISION x_centered = diff.x - 0.5;
  FP_PRECISION y_centered = diff.y - 0.5;

  // Compute the Gaussian weight
  FP_PRECISION exponent = -(x_centered * x_centered + y_centered * y_centered) /
                   (2 * sigma * sigma);
  FP_PRECISION weight = std::exp(exponent) / (2 * M_PI * sigma * sigma);

  return weight;
}