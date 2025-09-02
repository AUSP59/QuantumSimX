
all: build
	cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
	cmake --build build -j

test:
	cmake --build build --target test

install:
	cmake --install build --prefix /usr/local
