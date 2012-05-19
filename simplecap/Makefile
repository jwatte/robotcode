
all:	bld bld/testgrab bld/capdump

bld/%:	bld/%.o
	g++ -o $@ $^ -g -lv4l2

bld/%.o:	%.cpp
	g++ -c $< -o $@ -g -MMD

bld:
	mkdir -p bld

.PRECIOUS:	bld/%.o

-include $(wildcard bld/*.d)
