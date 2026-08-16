#pragma once

// Loading a SensorModel from a standalone JSON file.
//
// The sensor used to be declared inside the scene, under a camera. It is not
// any more: a sensor is a piece of hardware, not a property of a scene, and the
// whole point of writing the spectral cube out is that one render can be
// replayed through several cameras. Tying the sensor description to the scene
// file made that impossible without re-rendering.
//
// The file format is the old "Sensor" block, standalone, with the same key
// names and the same "_ref" semantics, so an existing scene's sensor block
// transfers over unchanged.
//
//   {
//     "_ref": "sensor:nikon_d700",
//     "_pattern": "RGGB",
//     "ExposureTime": "1e-4", "PixelPitch": "3.45e-6", "FNumber": "2.8",
//     "FullWell": "60000", "Gain": "16.0", "BitDepth": "12",
//     "DynamicRange": "12", "ReadNoise": "2.0", "DarkCurrent": "5.0",
//     "NoiseSources": "Shot Read Dark",
//     "QuantumEfficiency": {"_data": "400 0.3 550 0.6 700 0.4"},
//     "FilterRed": {...}, "FilterGreen": {...}, "FilterBlue": {...}
//   }
//
// Numbers may be JSON strings or JSON numbers. The scene format spells every
// value as a string because it is a mechanical XML transliteration; a config
// file written by hand has no such excuse, so both are accepted.

#include <string>

#include "SensorModel.hpp"

namespace sensor_config {

// Loads a sensor configuration. Throws std::runtime_error with a message naming
// the file and the problem -- an unknown Bayer pattern or an unresolvable
// spectral reference is fatal, never a warning, for the same reason it is in
// the scene loader: quietly substituting a default would corrupt a colour study
// invisibly.
//
// `spectra_directory` is passed to the spectral library if it has not been
// loaded yet; empty means use the usual search order (RAYTRACER_SPECTRA_DIR,
// then spectra/ beside the config, then spectra/ in the working directory).
SensorModel Load(const std::string& path,
                 const std::string& spectra_directory = "");

// Writes a sensor configuration back out, with every spectral curve expanded to
// explicit "_data" tables. Used to record exactly which sensor produced a RAW,
// including when the curves came from a library reference that might later
// change.
bool Save(const std::string& path, const SensorModel& sensor,
          std::string* error = nullptr);

}  // namespace sensor_config
