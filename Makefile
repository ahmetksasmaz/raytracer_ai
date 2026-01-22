# Common includes
INCLUDES = -I extern/ -I include/ -I include/BRDFs -I include/LightSources -I include/Materials -I include/Objects -I include/Textures -I include/ToneMappingAlgorithms
SOURCES = extern/*.cpp src/*.cpp src/*/*.cpp

# Optimized release flags (keeping RTTI and exceptions for compatibility)
RELEASE_FLAGS = -std=c++17 -O3 -march=native -mtune=native \
	-ffast-math -funroll-loops -ftree-vectorize \
	-fomit-frame-pointer \
	-flto \
	-DNDEBUG -w

# Debug flags
DEBUG_FLAGS = -std=c++17 -g -O0 -w

release:
	g++ $(INCLUDES) $(SOURCES) -o raytracer $(RELEASE_FLAGS)

debug:
	g++ $(INCLUDES) $(SOURCES) -o raytracer_debug $(DEBUG_FLAGS)

profile:
	g++ $(INCLUDES) $(SOURCES) -o raytracer_profile -std=c++17 -O2 -g -march=native -ffast-math -w

clean:
	rm -f raytracer raytracer_debug raytracer_profile