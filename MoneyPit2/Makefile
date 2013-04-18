
SRCS_lib:=$(wildcard lib/*.cpp)
OBJS_lib:=$(patsubst %.cpp,bld/%.o,$(SRCS_lib))
TOOLS:=$(patsubst tools/%,%,$(wildcard tools/*))
TOOL_BINS:=$(patsubst %,bld/obj/%,$(TOOLS))
TOOL_OBJS:=
APPS:=robot control
APP_BINS:=$(patsubst %,bld/obj/%,$(APPS))
APP_OBJS:=
LIB:=bld/obj/libmpit2.a

OPT?=-O0
CFLAGS:=$(OPT) -gdwarf-4 -fdebug-types-section -fvar-tracking-assignments -std=gnu++11 -march=native -Wall -Werror -pipe -Ilib -I/usr/include/libusb-1.0
LFLAGS:=-gdwarf-4 -lusb-1.0 -Lbld/obj -lmpit2 -lboost_system -lboost_thread -lfltk -ljpeg -lv4l2 -lGL -lglut -lpthread

AVR_SRCS:=$(wildcard avr/*/*.cpp)
LIBAVR_SRCS:=$(filter avr/libavr/%,$(AVR_SRCS))
AVR_SRCS:=$(filter-out avr/libavr/%,$(AVR_SRCS))
AVR_OBJS:=$(patsubst avr/%.cpp,bld/avrobj/%.o,$(AVR_SRCS))
AVR_BINS:=$(patsubst avr/%/,%,$(sort $(dir $(AVR_SRCS))))
AVR_CFLAGS:=-Wall -Werror -Wno-switch -Os -O3 -mcall-prologues -Iavr/libavr -std=gnu++0x -ffunction-sections
AVR_LFLAGS:=-Lbld/avrbin -lc

AVR_PARTS:=attiny84a atmega328p attiny85
#AVR_PROG_PORT?=usb
#AVR_PROG:=-c avrisp2
#AVR_PROG_PORT?=/dev/ttyACM0 -b19200
#AVR_PROG:=-c stk500v1
AVR_PROG_PORT?=usb
AVR_PROG:=-c usbtiny

ifneq ($(AVR_PROG_PORT),)
AVR_PROG+= -P $(AVR_PROG_PORT)
endif
AVR_OBJS+=$(foreach i,$(AVR_PARTS),$(patsubst avr/libavr/%.cpp,bld/avrobj/libavr_$i/%.o,$(LIBAVR_SRCS)))

AVR_HEXS:=$(patsubst %,bld/avrbin/%.hex,$(AVR_BINS))


all:	$(LIB) $(TOOL_BINS) $(APP_BINS) $(AVR_HEXS) tests

clean:	delbld

tests:
	#bld/obj/test1 2>&1

delbld:
	rm -rf bld

all clean:
	make -C LUFA $@

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
	make -C LUFA/MoneyPit2
	(cd LUFA/MoneyPit2 && ./flash.sh)

$(LIB):	$(OBJS_lib)
	@mkdir -p bld/obj
	ar cr $@ $(OBJS_lib)

bld/%.o:	%.cpp
	@mkdir -p $(dir $@)
	g++ -c -o $@ $< $(CFLAGS) -MMD

PART_blink:=atmega328p
PART_mp2_motor:=atmega328p
PART_mp2_counters:=atmega328p

fuses_blink:	fuses_20
fuses_mp2_motor:	fuses_20
fuses_mp2_counters:	fuses_20

define mkavrbin
bld/avrbin/$(1):	$$(filter bld/avrobj/$(1)/%,$(AVR_OBJS)) bld/avrbin/libavr_$$(PART_$(1)).a
	mkdir -p `dirname $$@`
	avr-gcc -o bld/avrbin/$(1) $(filter bld/avrobj/$(1)/%,$(AVR_OBJS)) $$(filter-out $$(SUPPRESS_LIBS_$(1)),-lavr_$$(PART_$(1)) $(AVR_LFLAGS)) -mmcu=$$(PART_$(1))
bld/avrbin/$(1).hex:	bld/avrbin/$(1)
	rm -f $$@
	mkdir -p `dirname $$@`
	avr-objcopy -R .eeprom -O ihex --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 $$< $$@
	avr-objcopy -R .eeprom -O ihex $$< $$@
endef
$(foreach avrbin,$(AVR_BINS),$(eval $(call mkavrbin,$(avrbin))))

bld/avrobj/%.o:	avr/%.cpp
	@mkdir -p `dirname $@`
	avr-gcc -mmcu=$(PART_$(patsubst %/,%,$(dir $(patsubst avr/%,%,$<)))) $(AVR_CFLAGS) -c $< -o $@ -MMD -I/usr/lib/avr/include

define build_libavr
bld/avrobj/libavr_$(1)/%.o:	avr/libavr/%.cpp
	@mkdir -p `dirname $$@`
	avr-gcc -mmcu=$(1) $(AVR_CFLAGS) -c $$< -o $$@ -MMD -I/usr/lib/avr/include
bld/avrbin/libavr_$(1).a:	$$(filter bld/avrobj/libavr_$(1)/%.o,$(AVR_OBJS))
	@mkdir -p `dirname $$@`
	ar cr $$@ $$^
endef

$(foreach i,$(AVR_PARTS),$(eval $(call build_libavr,$(i))))

TRANSLATE_attiny84a:=t84
TRANSLATE_attiny85:=t85
TRANSLATE_atmega328p:=m328p
define translate_part
$(if $(TRANSLATE_$(1)),$(TRANSLATE_$(1)),$(1))
endef

FUSES_B?=-B 10 -i 2
FLASH_B?=-B 1 -i 1

%:	bld/avrbin/%.hex fuses_%
	avrdude -u -V -p $(call translate_part,$(PART_$@)) -b 115200 $(FLASH_B) $(AVR_PROG) -U flash:w:$<:i
	avr-size bld/avrbin/$@

fuses_8:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xD9:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_tiny_8:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p t84 -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xDD:m -U efuse:w:0x01:m

fuses_8_85:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p t85 -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xDD:m -U efuse:w:0xFF:m

fuses_8_84:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p t84 -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xDD:m -U efuse:w:0x01:m

fuses_16:
	# 16 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_20:
	# 20 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_12:
	# 12 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 $(FUSES_B) $(AVR_PROG) -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m


-include $(patsubst %.o,%.d,$(OBJS_lib) $(TOOL_OBJS) $(APP_OBJS))
-include $(patsubst %.o,%.d,$(AVR_OBJS))
