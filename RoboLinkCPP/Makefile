
CPPFILES:=$(wildcard src/*.cpp)
OFILES:=$(patsubst %.cpp,bld/%.o,$(CPPFILES))
CFLAGS:=-g -O1
LIBS:=-lv4l2

bld/RoboLinkCPP:	$(OFILES)
	g++ -o $@ $(OFILES) $(LIBS)

clean:
	rm -rf bld
	mkdir bld

bld/%.o:	%.cpp
	@mkdir -p `dirname $@`
	g++ -MMD -c -o $@ $< $(CFLAGS)

-include $(patsubst %.o,%.d,$(OFILES))
