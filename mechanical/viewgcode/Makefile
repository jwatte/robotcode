
CC:=g++
CFLAGS:=-O1 -g -pipe -march=native -mtune=native -ffunction-sections -std=gnu++11 -MMD
LFLAGS:=-lGL -lGLU -lglut -lm
SRCS:=$(wildcard src/*.cpp)
OBJS:=$(patsubst src/%.cpp,bld/%.o,$(SRCS))

bld/vgc:	$(OBJS)
	$(CC) -o $@ -g $(OBJS) $(LFLAGS)

clean:
	rm -rf bld

bld/%.o:	src/%.cpp
	@mkdir -p bld
	$(CC) -c $< -o $@ $(CFLAGS)

-include $(patsubst %.o,%.d,$(OBJS))
