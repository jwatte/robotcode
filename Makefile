
SRCS:=$(wildcard */*.cpp)
OBJS:=$(patsubst %.cpp,bld/obj/%.o,$(SRCS))
APPS:=$(filter-out bld Makefile,$(wildcard *))

all:	bld/bin bld/obj $(patsubst %,bld/bin/%,$(APPS))

clean:
	rm -rf bld
	mkdir bld

define mkapp
bld/bin/$(1):	$(filter bld/obj/$(1)/%,$(OBJS))
	g++ -o bld/bin/$(1) $(filter bld/obj/$(1)/%,$(OBJS)) -lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_calib3d -lopencv_features2d -lopencv_video -lwebcam
endef

$(foreach app,$(APPS),$(eval $(call mkapp,$(app))))

bld/obj/%.o:	%.cpp
	mkdir -p `dirname $@`
	g++ -g -O2 -c $< -o $@ -MMD -Ilib

bld/obj:
	mkdir -p bld/obj
bld/bin:
	mkdir -p bld/bin

-include $(patsubst %.o,%.d,$(OBJS))
