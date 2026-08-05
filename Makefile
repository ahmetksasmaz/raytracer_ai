# Common includes
INCLUDES = -I extern/ -I include/ -I include/BRDFs -I include/LightSources -I include/Materials -I include/Objects -I include/Textures -I include/ToneMappingAlgorithms -I include/Sensor
SOURCES = extern/*.cpp src/*.cpp src/*/*.cpp

# Optimized release flags (keeping RTTI and exceptions for compatibility)
# -fno-finite-math-only is required: plain -ffast-math implies
# -ffinite-math-only, which lets the compiler assume NaN and Inf never occur and
# fold every std::isfinite() check to true. Verified on this toolchain --
# without it, isfinite(0.0/0.0) returns 1 and isnan(0.0/0.0) returns 0, so the
# NaN guards throughout the path tracer and tone mappers do nothing at all.
RELEASE_FLAGS = -std=c++17 -O3 -march=native -mtune=native \
	-ffast-math -fno-finite-math-only -funroll-loops -ftree-vectorize \
	-fomit-frame-pointer \
	-flto \
	-DNDEBUG -w

# Debug flags
DEBUG_FLAGS = -std=c++17 -g -O0 -w

release:
	g++ $(INCLUDES) $(SOURCES) -o raytracer $(RELEASE_FLAGS)

# EXR assertion tool used by tests/run_tests.sh. Built separately from the
# renderer because it has its own main() and its own TINYEXR_IMPLEMENTATION.
imgdiff: tools/imgdiff.cpp
	g++ -std=c++17 -O2 -w tools/imgdiff.cpp -o imgdiff

# Self-checks for the spectral core: CIE tables, illuminant chromaticities and
# the colour-conversion identities the rendering tests depend on.
spectraltest: tools/spectraltest.cpp include/Spectrum.hpp include/SpectralData.hpp
	g++ -std=c++17 -O2 -w tools/spectraltest.cpp -o spectraltest

# Statistical self-checks for the sensor model: noise moments, radiometric
# linearity, Bayer layout, quantisation.
sensortest: tools/sensortest.cpp include/Sensor/SensorModel.hpp include/Spectrum.hpp
	g++ -std=c++17 -O2 -w -I include -I include/Sensor tools/sensortest.cpp -o sensortest

test: release imgdiff spectraltest sensortest
	./tests/run_tests.sh

debug:
	g++ $(INCLUDES) $(SOURCES) -o raytracer_debug $(DEBUG_FLAGS)

profile:
	g++ $(INCLUDES) $(SOURCES) -o raytracer_profile -std=c++17 -O2 -g -march=native -ffast-math -w

clean:
	rm -f raytracer raytracer_debug raytracer_profile imgdiff spectraltest sensortest