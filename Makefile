release:
	g++ -I extern/ -I include/ -I include/BRDFs -I include/LightSources -I include/Materials -I include/Objects -I include/Textures -I include/ToneMappingAlgorithms extern/*.cpp src/*.cpp src/*/*.cpp -o raytracer -std=c++14 -O3 -w
debug:
	g++ -I extern/ -I include/ -I include/BRDFs -I include/LightSources -I include/Materials -I include/Objects -I include/Textures -I include/ToneMappingAlgorithms extern/*.cpp src/*.cpp src/*/*.cpp -o raytracer_debug -std=c++14 -g -w
clean:
	rm -f raytracer*
	rm -f raytracer_debug*