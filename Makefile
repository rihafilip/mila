LD=g++
LDFLAGS=
CXX=g++
CXXFLAGS=-std=c++20 -Wall -pedantic -Wno-long-long -g
LIBS=

BIN=mila

SRC=$(shell find src -name '*.cpp')
HEAD=$(shell find src -name '*.h')
OBJ=$(patsubst src%, build%, $(patsubst %.cpp, %.o, $(SRC)))

OBJDIRS=$(dir $(OBJ))
DUMMY:=$(shell mkdir --parents $(OBJDIRS))

## Compile
.PHONY: all run runtests clean gendoc
all: $(BIN)

$(BIN): $(OBJ)
	$(LD) $(LDFLAGS) -o $@ $^ $(LIBS)

run: $(BIN)
	./$(BIN)

runtests: $(BIN)
	./runtests

clean:
	rm -frd $(OBJ) $(BIN) Makefile.d doc

gendoc: $(SRC) $(HEAD) Doxyfile
	doxygen

## Rules
build/%.o: src/%.cpp
	$(CXX) $(CXXFLAGS) -c -o $@ $< $(LIBS)

Makefile.d: $(SRC) $(HEAD)
	$(CXX) -MM $(SRC) | sed -E 's/(^.*\.o:)/build\/\1/' > $@

-include Makefile.d
