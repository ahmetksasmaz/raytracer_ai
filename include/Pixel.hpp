#pragma once

class Pixel {
 public:
  Pixel(float r, float g, float b) : r_(r), g_(g), b_(b) {}
  ~Pixel() {}
  float r_;
  float g_;
  float b_;
};