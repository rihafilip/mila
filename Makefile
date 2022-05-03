CMAKE_MAKEFILE=build/Makefile

SRC=$(shell find src -name '*.cpp')
HEAD=$(shell find src -name '*.h')

## Compile
.PHONY: all mila cmake runtests clean doc
all: mila
mila: $(CMAKE_MAKEFILE)
	cd build && make

cmake:
	mkdir -p build
	cd build && cmake .. -DCMAKE_BUILD_TYPE=Debug

runtests: mila
	./runtests

clean:
	rm -frd build/ doc/

doc: $(SRC) $(HEAD) Doxyfile
	doxygen

