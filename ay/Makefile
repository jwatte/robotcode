
PROG_CPP := $(wildcard prog/*.cpp)
LIB_CPP := $(wildcard lib/*.cpp)
LIB := bld/libay.a
PROG := $(patsubst prog/%.cpp,bld/%,$(PROG_CPP))
OPT ?= -O2
PROG_OBJ := $(patsubst %.cpp,bld/%.o,$(PROG_CPP))
LIB_OBJ := $(patsubst %.cpp,bld/%.o,$(LIB_CPP))

all:	bld bld/prog bld/lib $(LIB) $(PROG)

clean:
	rm -rf bld

$(LIB):	$(LIB_OBJ)
	ar cr $@ $(LIB_OBJ)

bld/%:	bld/prog/%.o $(LIB)
	g++ -g -o $@ $< -Lbld -lay -ljpeg -lm

bld/lib/%.o:	lib/%.cpp
	g++ -c -g -MMD $(OPT) -Ilib -o $@ $<

bld/prog/%.o:	prog/%.cpp
	g++ -c -g -MMD $(OPT) -Ilib -o $@ $<

bld:
	mkdir -p bld
bld/prog:
	mkdir -p bld/prog
bld/lib:
	mkdir -p bld/lib

-include $(patsubst %.o,%.d,$(PROG_OBJ) $(LIB_OBJ))
