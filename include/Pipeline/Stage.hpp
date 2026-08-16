#pragma once

// Shared plumbing for the pipeline stage programs.
//
// Every stage has the same shape -- read one file, apply one transformation,
// write one file -- so the argument parsing, the error reporting and the
// conversion between the on-disk plane layout and the working buffers live here
// rather than being written out a dozen times. What is left in each main() is
// the stage's own physics.
//
// The convention every stage follows:
//
//   <prog> --in <file> --out <file> [--config <sensor.json>] [flags]
//
// Files chain by postfix: <scene>_<stage>.<ext>. Nothing enforces the naming --
// a stage reads whatever you point it at -- but tools/pipeline.sh follows it,
// and the postfix is what makes an intermediate identifiable on disk.

#include <string>
#include <vector>

#include "Core/Types.hpp"
#include "ImageIO/ImageIO.hpp"
#include "Spectrum.hpp"

namespace pipeline {

// Command-line arguments in the uniform form above.
class Arguments {
 public:
  Arguments(int argc, char** argv);

  // Returns the value of --<name>, or `fallback` if it was not given.
  std::string Get(const std::string& name, const std::string& fallback = "") const;
  bool Has(const std::string& name) const;
  FP_PRECISION Number(const std::string& name, FP_PRECISION fallback) const;
  int Integer(const std::string& name, int fallback) const;

  const std::string& program() const { return program_; }

 private:
  std::string program_;
  std::vector<std::pair<std::string, std::string>> values_;
};

// Prints `usage` to stderr and returns 1, so a main() can `return Usage(...)`.
int Usage(const std::string& program, const std::string& usage);

// Reports a failure on stderr, prefixed with the stage name, and returns 1.
int Fail(const std::string& program, const std::string& message);

// --- Reading ----------------------------------------------------------------
// All three read an EXR and complain in the stage's voice if the channel layout
// is not what this stage expects, which is the most common way to mis-wire a
// pipeline: pointing a 3-channel stage at a 31-band cube.

// A single-channel image (a mosaic, a RAW, a linearised RAW).
bool ReadSingle(const std::string& path, int* width, int* height,
                std::vector<FP_PRECISION>* values, std::string* error);

// A three-channel R/G/B image in whatever space the producing stage was in.
bool ReadTriple(const std::string& path, int* width, int* height,
                std::vector<FP_PRECISION> rgb[3], std::string* error);

// An N-band spectral cube, resampled onto the compiled-in band grid if the file
// was written with a different one. The wavelengths come from the channel
// names, never from the channel count.
bool ReadSpectral(const std::string& path, int* width, int* height,
                  std::vector<Spectrum>* spectra, std::string* error);

// --- Writing ----------------------------------------------------------------
bool WriteSingle(const std::string& path, int width, int height,
                 const std::vector<FP_PRECISION>& values,
                 const std::string& channel_name, std::string* error);

bool WriteTriple(const std::string& path, int width, int height,
                 const std::vector<FP_PRECISION> rgb[3], std::string* error);

bool WriteSpectral(const std::string& path, int width, int height,
                   const std::vector<Spectrum>& spectra, std::string* error);

}  // namespace pipeline
