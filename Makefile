CMAKE_MAKEFILE=build/Makefile

SRC=$(shell find src -name '*.cpp')
HEAD=$(shell find src -name '*.h')

## Compile
.PHONY: all mila cmake
all: mila
mila: $(CMAKE_MAKEFILE)
	make -C "./build"

# Implicit CMake generate
$(CMAKE_MAKEFILE):
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug

# Explicit CMake generate
cmake:
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug


## Run tests
.PHONY: runtests
runtests: mila
	./runtests

## Misc
.PHONY: clean doc
clean:
	rm -frd build/ doc/

doc: $(SRC) $(HEAD) Doxyfile
	doxygen

