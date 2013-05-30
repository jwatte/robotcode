
SRCS_lib:=$(wildcard lib/*.cpp)
OBJS_lib:=$(patsubst %.cpp,bld/%.o,$(SRCS_lib))
TOOLS:=$(patsubst tools/%,%,$(wildcard tools/*))
TOOL_BINS:=$(patsubst %,bld/obj/%,$(TOOLS))
TOOL_OBJS:=
APPS:=robot control
APP_BINS:=$(patsubst %,bld/obj/%,$(APPS))
APP_OBJS:=
LIB:=bld/obj/libonyx.a

OPT?=-O0
CFLAGS:=$(OPT) -gdwarf-4 -fdebug-types-section -fvar-tracking-assignments -std=gnu++11 -march=native -Wall -Werror -pipe -Ilib -I/usr/include/libusb-1.0
LFLAGS:=-gdwarf-4 -lusb-1.0 -Lbld/obj -lonyx -lboost_system -lboost_thread -lfltk -ljpeg -lv4l2 -lGL -lglut -lpthread

all:	$(LIB) $(TOOL_BINS) $(APP_BINS) tests

clean:	delbld

tests:
	#bld/obj/test1 2>&1

delbld:
	rm -rf bld

all clean:
	make -C LUFA $@ && make -C powerboard $@

define mk_tool
SRCS_$(1):=$$(wildcard tools/$(1)/*.cpp)
OBJS_$(1):=$$(patsubst %.cpp,bld/%.o,$$(SRCS_$(1)))
TOOL_OBJS+=$$(OBJS_$(1))
bld/obj/$(1):	$$(OBJS_$(1)) $(LIB)
	g++ -o $$@ $$(OBJS_$(1)) $$(LFLAGS)
endef

$(foreach tool,$(TOOLS),$(eval $(call mk_tool,$(tool))))

define mk_app
SRCS_$(1):=$$(wildcard $(1)/*.cpp)
OBJS_$(1):=$$(patsubst %.cpp,bld/%.o,$$(SRCS_$(1)))
APP_OBJS+=$$(OBJS_$(1))
bld/obj/$(1):	$$(OBJS_$(1)) $(LIB)
	g++ -o $$@ $$(OBJS_$(1)) $$(LFLAGS)
endef

$(foreach app,$(APPS),$(eval $(call mk_app,$(app))))

flash:
	make -C LUFA/OnyxWalker
	(cd LUFA/OnyxWalker && ./flash.sh)

$(LIB):	$(OBJS_lib)
	@mkdir -p bld/obj
	ar cr $@ $(OBJS_lib)

bld/%.o:	%.cpp
	@mkdir -p $(dir $@)
	g++ -c -o $@ $< $(CFLAGS) -MMD

-include $(patsubst %.o,%.d,$(OBJS_lib) $(TOOL_OBJS) $(APP_OBJS))
