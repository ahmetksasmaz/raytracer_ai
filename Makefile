all:
debug:
	g++ -I extern/ -I include/ extern/*.cpp src/*.cpp src/*/*.cpp -o raytracer_debug -std=c++11 -g -w
release:
	g++ -I extern/ -I include/ extern/*.cpp src/*.cpp src/*/*.cpp -o raytracer -std=c++11 -O3 -w
clean:
	rm -f raytracer
	rm -f raytracer_debug