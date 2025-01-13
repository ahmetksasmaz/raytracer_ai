#pragma once
#include "../extern/parser.h"
#include <cmath>
#include <iostream>

using namespace parser;

class BaseSpectrum {
 public:
  BaseSpectrum(std::vector<float> samples, const int min_wavelength,
               const int max_wavelength)
      : samples_(samples),
        min_wavelength_(min_wavelength),
        max_wavelength_(max_wavelength) {
          samples_sum_ = 0.0f;
          for(auto & sample : samples_){
            samples_sum_ += sample;
          }
        }
  virtual ~BaseSpectrum() = default;

  float operator[](const int wavelength) const {
    if (wavelength < min_wavelength_ || wavelength > max_wavelength_) {
      return 0;
    }

    float index_f = (float(wavelength - min_wavelength_) /
                     (max_wavelength_ - min_wavelength_)) *
                    samples_.size();

    int index = static_cast<int>(index_f);
    float floor = std::floor(index_f);
    float low = samples_[index];
    float high = samples_[index + 1];
    return low + (high - low) * (index_f - floor);
  }

  float operator()(int wavelength, float power){
    return (*this)[wavelength] * power / samples_sum_;
  }

  std::vector<int> GetSamples() const {
    std::vector<int> wavelengths;
    float step = (max_wavelength_ - min_wavelength_) / float(samples_.size());
    for (int wavelength = min_wavelength_; wavelength <= max_wavelength_; wavelength += step) {
      wavelengths.push_back(wavelength);
    }
    return wavelengths;
  }

 private:
  std::vector<float> samples_;
  const int min_wavelength_;
  const int max_wavelength_;
  float samples_sum_;
};