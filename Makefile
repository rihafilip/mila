CMAKE_MAKEFILE=build/Makefile

SRC=$(shell find src -name '*.cpp')
HEAD=$(shell find src -name '*.h')

SAMPLES=./samples
TEST_FILE=consts

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
.PHONY: runtests test
runtests: mila
	./runtests

test: mila
	cd $(SAMPLES) && ../mila $(TEST_FILE).mila -o $(TEST_FILE)

## Misc
.PHONY: clean doc
clean:
	rm -frd build/ doc/

doc: $(SRC) $(HEAD) Doxyfile
	doxygen

