
SRCS_lib:=$(wildcard lib/*.cpp)
OBJS_lib:=$(patsubst %.cpp,bld/%.o,$(SRCS_lib))
TOOLS:=$(patsubst tools/%,%,$(wildcard tools/*))
TOOL_BINS:=$(patsubst %,bld/obj/%,$(TOOLS))

CFLAGS:=-O0 -gdwarf-4 -fdebug-types-section -fvar-tracking-assignments -std=gnu++11 -march=native -Wall -Werror -Ilib -I/usr/include/libusb-1.0
LFLAGS:=-gdwarf-3 -lusb-1.0 -Lbld/obj -lonyx -lboost_system -lboost_thread -lfltk

all:	bld/obj/libonyx.a $(TOOL_BINS)

clean:	delbld

delbld:
	rm -rf bld

all clean:
	make -C LUFA $@
	make -C misc $@
	make -C control $@

define mk_tool
SRCS_$(1):=$$(wildcard tools/$(1)/*.cpp)
OBJS_$(1):=$$(patsubst %.cpp,bld/%.o,$$(SRCS_$(1)))
bld/obj/$(1):	$$(OBJS_$(1)) bld/obj/libonyx.a
	g++ -o $$@ $$(OBJS_$(1)) $$(LFLAGS)
endef

$(foreach tool,$(TOOLS),$(eval $(call mk_tool,$(tool))))

flash:
	make -C LUFA/OnyxWalker
	(cd LUFA/OnyxWalker && ./flash.sh)

bld/obj/libonyx.a:	$(OBJS_lib)
	@mkdir -p bld/obj
	ar cr $@ $(OBJS_lib)

bld/%.o:	%.cpp
	@mkdir -p $(dir $@)
	g++ -c -o $@ $< $(CFLAGS) -MMD

-include $(patsubst %.o,%.d,$(OBJS_lib))
