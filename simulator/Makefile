.PHONY: all configure run test clean

all: build/bluebus-cd53-sim

configure:
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug

build/bluebus-cd53-sim: configure
	cmake --build build --parallel

run: build/bluebus-cd53-sim
	./build/bluebus-cd53-sim

test: build/bluebus-cd53-sim
	ctest --test-dir build --output-on-failure

clean:
	cmake -E remove_directory build
