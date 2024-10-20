#pragma once

// #define DEBUG

#include <cmath>
#include <memory>

#include "../extern/parser.h"

using namespace parser;

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

inline Vec3f operator-(Vec3f a) { return Vec3f{-a.x, -a.y, -a.z}; }

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