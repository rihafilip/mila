CMAKE_MAKEFILE=build/Makefile

SRC=$(shell find src -name '*.cpp')
HEAD=$(shell find src -name '*.h')

SAMPLES=./samples
TEST ?= expressions2

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
	cd $(SAMPLES) && ../mila $(TEST).mila -o $(TEST)

## Misc
.PHONY: clean doc
clean:
	rm -frd build/ doc/
	cd $(SAMPLES) && ls | grep -v '.*\.mila' | xargs rm

doc: $(SRC) $(HEAD) Doxyfile
	doxygen

