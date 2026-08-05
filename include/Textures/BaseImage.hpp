#pragma once
#include "../extern/parser.h"
#include "Helper.hpp"

using namespace parser;

class BaseImage {
 public:
  BaseImage(const std::string& path);
  virtual ~BaseImage() = default;

  int MaxLevel() const { return static_cast<int>(mipmapping_.size()) - 1; }

  // Texture coordinates wrap (textures tile), but the wrap is computed against
  // the real extent of the requested mip level rather than width_ >> level, and
  // it handles negative operands. Both matter: a NaN coordinate casts to a
  // garbage int, C++ gives % a negative result for a negative left operand, and
  // the smallest mip level can have a zero extent -- each of which previously
  // produced an out-of-bounds read or a division by zero.
  Vec3f operator()(int x, int y, int level = 0) const {
    level = std::min(std::max(level, 0), MaxLevel());
    const int level_height = static_cast<int>(mipmapping_[level].size());
    if (level_height <= 0) return Vec3f{0, 0, 0};
    const int level_width = static_cast<int>(mipmapping_[level][0].size());
    if (level_width <= 0) return Vec3f{0, 0, 0};
    return mipmapping_[level][WrapIndex(y, level_height)]
                             [WrapIndex(x, level_width)];
  }

  int width_;
  int height_;

 protected:
  std::vector<std::vector<std::vector<Vec3f>>> mipmapping_;
  const std::string path_;
};