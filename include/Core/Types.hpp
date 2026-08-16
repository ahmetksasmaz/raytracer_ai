#pragma once

// The scalar type and the small vector types, split out of extern/parser.h.
//
// The split exists so the sensor and ISP stages can be built without the scene
// description. They need exactly two things from the parser header -- the
// scalar typedef and the vector structs -- but including parser.h to get them
// also drags in every Raw* scene struct and the PLY reader, which a program
// that only reads an EXR and writes an EXR has no business linking against.
//
// parser.h includes this file, so the types are still reachable by their old
// spelling everywhere in the renderer and nothing downstream had to change.

#include <cstddef>
#include <ostream>

#define FP_PRECISION double

namespace parser {

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

}  // namespace parser
