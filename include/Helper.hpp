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
#include <cstdint>
#include <thread>

#include "../extern/parser.h"

using namespace parser;

struct FastRandomNumberGenerator {
  uint64_t state;
  
  FastRandomNumberGenerator(uint64_t seed = 0) {
    state = seed != 0 ? seed : 0x853c49e6748fea9bULL;
  }
  
  inline uint64_t next() {
    uint64_t x = state;
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    state = x;
    return x * 0x2545F4914F6CDD1DULL;
  }
  
  inline FP_PRECISION NextDouble() {
    return static_cast<FP_PRECISION>(next() >> 11) * (1.0 / 9007199254740992.0);
  }
  
  inline int NextInteger(int max) {
    return static_cast<int>(next() % max);
  }
};

inline thread_local FastRandomNumberGenerator ThreadLocalRandomNumberGenerator(std::hash<std::thread::id>{}(std::this_thread::get_id()));

inline FP_PRECISION FastRandom() {
  return ThreadLocalRandomNumberGenerator.NextDouble();
}

inline int FastRandomInteger(int max) {
  return ThreadLocalRandomNumberGenerator.NextInteger(max);
}

// The old implementation guarded the bit-hack branch with
// `#if FP_PRECISION == double`, which compares two identifiers the preprocessor
// does not know -- both evaluate to 0, so the condition was always true and the
// bit-hack was dead code. It would have been undefined behaviour on a double
// anyway (it type-puns through int), and on any modern target rsqrt in hardware
// beats it. Kept as a named function because call sites read better with it.
inline FP_PRECISION FastInverseSquareRoot(FP_PRECISION x) {
  return 1.0 / std::sqrt(x);
}

// Standard normal via Box-Muller, on the thread-local generator.
inline FP_PRECISION SampleGaussian(FP_PRECISION mean, FP_PRECISION stddev) {
  if (stddev <= 0.0) return mean;
  // Guard the log against exactly zero.
  const FP_PRECISION u1 = std::max(FastRandom(), static_cast<FP_PRECISION>(1e-12));
  const FP_PRECISION u2 = FastRandom();
  return mean + stddev * std::sqrt(-2.0 * std::log(u1)) * std::cos(2.0 * M_PI * u2);
}

// Poisson draw, used for photon shot noise and dark current.
//
// Two regimes, which is what makes this usable across the whole exposure range:
// Knuth's product method is exact but costs O(lambda) draws, so it is used only
// for small counts. Above the threshold the distribution is well approximated by
// a Gaussian of matching mean and variance (sigma = sqrt(lambda)), which is the
// standard treatment and avoids an unbounded loop on a bright pixel where
// lambda can reach millions.
inline FP_PRECISION SamplePoisson(FP_PRECISION lambda) {
  if (!(lambda > 0.0)) return 0.0;

  constexpr FP_PRECISION kExactThreshold = 30.0;
  if (lambda < kExactThreshold) {
    const FP_PRECISION limit = std::exp(-lambda);
    FP_PRECISION product = FastRandom();
    int count = 0;
    while (product > limit) {
      count++;
      product *= FastRandom();
    }
    return static_cast<FP_PRECISION>(count);
  }

  // Round so the result stays a whole number of electrons, and clamp: the
  // Gaussian tail can go negative where the Poisson cannot.
  const FP_PRECISION approx = SampleGaussian(lambda, std::sqrt(lambda));
  return std::max(static_cast<FP_PRECISION>(0.0), std::floor(approx + 0.5));
}

inline Vec3f FastNormalize(Vec3f a) {
  FP_PRECISION inverse_norm = FastInverseSquareRoot(a.x * a.x + a.y * a.y + a.z * a.z);
  return Vec3f{a.x * inverse_norm, a.y * inverse_norm, a.z * inverse_norm};
}

inline void BuildOrthonormalBasis(const Vec3f& n, Vec3f& tangent, Vec3f& bitangent) {
  if (n.z < -0.9999f) {
    tangent = Vec3f{0.0, -1.0, 0.0};
    bitangent = Vec3f{-1.0, 0.0, 0.0};
    return;
  }
  FP_PRECISION sign = n.z >= 0.0 ? 1.0 : -1.0;
  FP_PRECISION a = -1.0 / (sign + n.z);
  FP_PRECISION b = n.x * n.y * a;
  tangent = Vec3f{1.0 + sign * n.x * n.x * a, sign * b, -sign * n.x};
  bitangent = Vec3f{b, sign + n.y * n.y * a, -n.y};
}

const Mat4x4f IDENTITY_MATRIX = {
    {{1, 0, 0, 0}, {0, 1, 0, 0}, {0, 0, 1, 0}, {0, 0, 0, 1}}};

// Repeat-wrap an index into [0, extent). Unlike a bare `%` this copes with
// negative inputs (C++ keeps the sign of the dividend) and a zero extent.
inline int WrapIndex(int v, int extent) {
  if (extent <= 0) return 0;
  const int r = v % extent;
  return r < 0 ? r + extent : r;
}

inline Vec3f cross(Vec3f a, Vec3f b) {
  return Vec3f{a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z,
               a.x * b.y - a.y * b.x};
}

inline Vec3f normalize(Vec3f a) {
  FP_PRECISION norm = sqrt(a.x * a.x + a.y * a.y + a.z * a.z);
  return Vec3f{a.x / norm, a.y / norm, a.z / norm};
}

inline Vec2f normalize(Vec2f a) {
  FP_PRECISION norm = sqrt(a.x * a.x + a.y * a.y);
  return Vec2f{a.x / norm, a.y / norm};
}

inline FP_PRECISION norm2(Vec3f a) { return a.x * a.x + a.y * a.y + a.z * a.z; }

inline FP_PRECISION norm2(Vec2f a) { return a.x * a.x + a.y * a.y; }

inline FP_PRECISION norm(Vec3f a) { return sqrt(norm2(a)); }

inline FP_PRECISION norm(Vec2f a) { return sqrt(a.x * a.x + a.y * a.y); }

inline FP_PRECISION dot(Vec3f a, Vec3f b) { return a.x * b.x + a.y * b.y + a.z * b.z; }

inline Vec3f hadamard(Vec3f a, Vec3f b) {
  return Vec3f{a.x * b.x, a.y * b.y, a.z * b.z};
}

inline Vec2f operator*(Vec2f a, FP_PRECISION b) {
  return Vec2f{a.x * b, a.y * b};
}

inline Vec3f operator*(Vec3f a, FP_PRECISION b) {
  return Vec3f{a.x * b, a.y * b, a.z * b};
}

inline Vec2f operator*(FP_PRECISION a, Vec2f b) {
  return Vec2f{a * b.x, a * b.y};
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

inline Vec2f operator-(Vec2f a) { return Vec2f{-a.x, -a.y}; }

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

// Fisher-Yates. The index has to be drawn from the REMAINING range [i, size);
// drawing from the whole range every time produces a non-uniform permutation,
// which biases the stratification the samplers below are built to provide.
template <typename T>
inline void shuffle(std::vector<T>& samples) {
  if (samples.size() < 2) return;
  for (size_t i = 0; i + 1 < samples.size(); i++) {
    const size_t remaining = samples.size() - i;
    const size_t j = i + static_cast<size_t>(FastRandomInteger(static_cast<int>(remaining)));
    std::swap(samples[i], samples[j]);
  }
}

inline std::vector<FP_PRECISION> uniform_1d(int num_samples) {
  std::vector<FP_PRECISION> samples;
  samples.reserve(num_samples);
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((FP_PRECISION)i / num_samples);
  }
  shuffle(samples);
  return samples;
}

// CONTRACT: every 2D sampler returns exactly num_samples samples.
//
// uniform_2d, jittered_2d and uniform_random_2d used to return num_samples
// SQUARED. Nothing broke only because the camera is wired to Hammersley, which
// returns num_samples -- switching that one line would have written n^2 samples
// into an n-slot buffer. The grid samplers now stratify over a ceil(sqrt(n))
// grid and hand back exactly n.
inline std::vector<Vec2f> uniform_2d(int num_samples) {
  std::vector<Vec2f> samples;
  if (num_samples <= 0) return samples;
  samples.reserve(num_samples);
  const int side = static_cast<int>(std::ceil(std::sqrt((double)num_samples)));
  for (int i = 0; i < side && (int)samples.size() < num_samples; i++) {
    for (int j = 0; j < side && (int)samples.size() < num_samples; j++) {
      samples.push_back(Vec2f{(FP_PRECISION)i / side, (FP_PRECISION)j / side});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<FP_PRECISION> uniform_random_1d(int num_samples) {
  std::vector<FP_PRECISION> samples;
  samples.reserve(num_samples);
  for (int i = 0; i < num_samples; i++) {
    samples.push_back(FastRandom());
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> uniform_random_2d(int num_samples) {
  std::vector<Vec2f> samples;
  if (num_samples <= 0) return samples;
  samples.reserve(num_samples);
  for (int i = 0; i < num_samples; i++) {
    samples.push_back(Vec2f{FastRandom(), FastRandom()});
  }
  return samples;  // already independent; shuffling adds nothing
}

inline std::vector<FP_PRECISION> jittered_1d(int num_samples) {
  std::vector<FP_PRECISION> samples;
  samples.reserve(num_samples);
  for (int i = 0; i < num_samples; i++) {
    samples.push_back((i + FastRandom()) / num_samples);
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> jittered_2d(int num_samples) {
  std::vector<Vec2f> samples;
  if (num_samples <= 0) return samples;
  samples.reserve(num_samples);
  const int side = static_cast<int>(std::ceil(std::sqrt((double)num_samples)));
  for (int i = 0; i < side && (int)samples.size() < num_samples; i++) {
    for (int j = 0; j < side && (int)samples.size() < num_samples; j++) {
      samples.push_back(Vec2f{(i + FastRandom()) / side,
                              (j + FastRandom()) / side});
    }
  }
  shuffle(samples);
  return samples;
}

inline std::vector<Vec2f> multi_jittered_2d(int num_samples) {
  std::vector<Vec2f> samples;
  if (num_samples <= 0) return samples;
  samples.reserve(num_samples);
  const int n = std::max(1, static_cast<int>(std::sqrt((double)num_samples)));
  const FP_PRECISION subcell_width = 1.0 / (n * n);
  for (int i = 0; i < n; i++) {
    for (int j = 0; j < n; j++) {
      samples.push_back(
          Vec2f{(i * n + j + FastRandom()) * subcell_width,
                (j * n + i + FastRandom()) * subcell_width});
    }
  }
  // n*n rarely equals num_samples exactly; top up so the contract holds.
  while ((int)samples.size() < num_samples) {
    samples.push_back(Vec2f{FastRandom(), FastRandom()});
  }
  samples.resize(num_samples);
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
  samples.reserve(num_samples);
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
  samples.reserve(num_samples);
  for (int i = 0; i < num_samples; i++) {
    FP_PRECISION x = (FP_PRECISION)radical_inverse(i, 2);
    FP_PRECISION y = (FP_PRECISION)radical_inverse(i, 3);
    samples.push_back(Vec2f{x, y});
  }
  shuffle(samples);
  return samples;
}

// Gaussian reconstruction weight for a sample sitting `centered_offset` PIXELS
// away from the centre of the pixel being reconstructed.
//
// The offset arrives already centred. This function used to subtract 0.5
// internally, which is only correct for a sample inside the pixel itself -- a
// sample from a neighbouring pixel needs that neighbour's integer offset added
// as well, and the caller is the only one that knows it.
inline FP_PRECISION gaussian_kernel_weight(Vec2f centered_offset, FP_PRECISION sigma) {
  const FP_PRECISION exponent =
      -(centered_offset.x * centered_offset.x + centered_offset.y * centered_offset.y) /
      (2 * sigma * sigma);
  return std::exp(exponent) / (2 * M_PI * sigma * sigma);
}

// Solid-angle pdf of the hemisphere sampler currently in use.
//
// This is the SINGLE SOURCE OF TRUTH for that pdf. The sampling routines below
// return it, and MIS weighting recomputes it for directions that were NOT drawn
// this way (a light-sampled direction still needs "what would the BSDF strategy
// have given this?"). If the two ever drift apart, MIS produces wrong weights
// silently -- no crash, no NaN, just a subtly wrong image. Hence one formula.
inline FP_PRECISION hemisphere_pdf(FP_PRECISION cos_theta, bool importance_sampling) {
  if (cos_theta <= 0.0) return 0.0;
  return importance_sampling ? cos_theta / M_PI : 1.0 / (2.0 * M_PI);
}

inline void uniform_hemisphere_sample(FP_PRECISION& theta, FP_PRECISION& phi, FP_PRECISION& pdf) {
  FP_PRECISION u1 = FastRandom();
  FP_PRECISION u2 = FastRandom();

  theta = std::acos(u1);
  phi = 2 * M_PI * u2;
  pdf = hemisphere_pdf(std::cos(theta), false);
}

inline void cosine_hemisphere_sample(FP_PRECISION& theta, FP_PRECISION& phi, FP_PRECISION& pdf) {
  FP_PRECISION u1 = FastRandom();
  FP_PRECISION u2 = FastRandom();

  theta = std::asin(std::sqrt(u1));
  phi = 2 * M_PI * u2;
  // No clamping here: the pdf must match hemisphere_pdf exactly. Callers skip
  // samples whose pdf is degenerate.
  pdf = hemisphere_pdf(std::cos(theta), true);
}