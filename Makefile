
APPS:=rl2 simplegps simpleusb #simplecap robolink

LIBUSB_LOC?=/usr/local
OPT?=-O3
CPP_OPT:=-ggdb $(OPT) -fvar-tracking-assignments -Wall -Werror -std=gnu++0x -msse -march=native -mtune=native
CPP_CFLAGS:=$(sort $(CPP_OPT) $(filter-out -D_FORTIFY_SOURCE%, $(filter-out -O%, \
    $(filter-out -mtune=%, $(filter-out -march=%,$(shell fltk-config --use-images --cxxflags)))))) \
    -I$(LIBUSB_LOC)/include/libusb-1.0
CPP_LFLAGS:=-ljpeg $(sort $(CPP_OPT) $(shell fltk-config --use-images --ldflags)) \
	-lv4l2 -lgps -lboost_thread -lboost_system $(LIBUSB_LOC)/lib/libusb-1.0.so
ALL_APP_CPP_SRCS:=$(foreach app,$(APPS),$(wildcard $(app)/*.cpp))
CPP_SRCS:=$(sort $(foreach app,$(APPS),$(filter-out $(app)/test_%.cpp, \
    $(filter $(app)/%.cpp, $(ALL_APP_CPP_SRCS)))))
CPP_OBJS:=$(patsubst %.cpp,bld/obj/%.o,$(CPP_SRCS))
CPP_TEST_SRCS:=$(sort $(foreach app,$(APPS),$(filter $(app)/test_%.cpp,$(ALL_APP_CPP_SRCS))))
CPP_TEST_OBJS:=$(patsubst %.cpp,bld/obj/%.o,$(CPP_TEST_SRCS))
CPP_TESTS:=$(sort $(foreach app,$(APPS),$(patsubst %.cpp,%, \
    $(filter $(app)/test_%.cpp,$(ALL_APP_CPP_SRCS)))))
TEST_NAMES:=$(foreach app,$(APPS),$(patsubst $(app)/%,%,$(filter $(app)/%,$(CPP_TESTS))))

AVR_SRCS:=$(wildcard avr/*/*.cpp)
LIBAVR_SRCS:=$(filter avr/libavr/%,$(AVR_SRCS))
AVR_SRCS:=$(filter-out avr/libavr/%,$(AVR_SRCS))
AVR_OBJS:=$(patsubst avr/%.cpp,bld/avrobj/%.o,$(AVR_SRCS))
AVR_BINS:=$(patsubst avr/%/,%,$(sort $(dir $(AVR_SRCS))))
AVR_CFLAGS:=-Wall -Werror -Wno-switch -Os -O3 -mcall-prologues -Iavr/libavr -std=gnu++0x -ffunction-sections
AVR_LFLAGS:=-Lbld/avrbin -lc

AVR_PARTS:=attiny84a atmega328p attiny85
AVR_PROG_PORT?=usb
AVR_PROG:=-c avrisp2
ifneq ($(AVR_PROG_PORT),)
AVR_PROG+= -P $(AVR_PROG_PORT)
endif
AVR_OBJS+=$(foreach i,$(AVR_PARTS),$(patsubst avr/libavr/%.cpp,bld/avrobj/libavr_$i/%.o,$(LIBAVR_SRCS)))


OPENCV_LFLAGS:=-lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_calib3d -lopencv_features2d -lopencv_video -lwebcam

all:	$(patsubst %,bld/bin/%,$(TEST_NAMES)) $(patsubst %,bld/bin/%,$(APPS)) $(patsubst %,bld/avrbin/%.hex,$(AVR_BINS))

clean:
	rm -rf bld
	mkdir bld

PART_motorboard:=atmega328p
PART_estop:=atmega328p
PART_usbboard:=atmega328p
PART_commusb:=atmega328p
PART_testuart:=atmega328p
PART_sensorboard:=atmega328p
PART_blink:=atmega328p
PART_stepdirsend:=atmega328p
PART_stepdirrecv:=atmega328p
PART_display:=atmega328p
PART_readcompass:=atmega328p
PART_test85:=attiny85

fuses_motorboard:	fuses_8
fuses_estop:	fuses_8
#fuses_usbboard:	fuses_16
fuses_commusb:	fuses_16
fuses_testuart:	fuses_16
fuses_sensorboard:	fuses_8
fuses_blink:	fuses_8
fuses_stepdirsend:	fuses_8
fuses_stepdirrecv:	fuses_8
fuses_display:	fuses_20
fuses_readcompass:	fuses_16
fuses_test85:	fuses_8_85

define mkapp
bld/bin/$(1):	$(filter bld/obj/$(1)/%,$(CPP_OBJS))
	@mkdir -p `dirname $$@`
	g++ -o bld/bin/$(1) $(filter bld/obj/$(1)/%,$(CPP_OBJS)) $(CPP_LFLAGS)
endef
$(foreach app,$(APPS),$(eval $(call mkapp,$(app))))

define mktest
bld/bin/$(2):	$(filter-out bld/obj/$(1)/main.o,$(filter bld/obj/$(1)/%,$(CPP_OBJS))) bld/obj/$(1)/$(2).o
	@mkdir -p `dirname $$@`
	g++ -o $$@ $$^ $(CPP_LFLAGS)
	echo Running bld/bin/$(2)
	@bld/bin/$(2)
.PRECIOUS:	bld/bin/$(2)
endef
$(foreach test,$(CPP_TESTS),$(eval $(call mktest,$(patsubst %/,%,$(dir $(test))),$(notdir $(test)))))


define mkavrbin
bld/avrbin/$(1):	$$(filter bld/avrobj/$(1)/%,$(AVR_OBJS)) bld/avrbin/libavr_$$(PART_$(1)).a
	mkdir -p `dirname $$@`
	avr-gcc -o bld/avrbin/$(1) $(filter bld/avrobj/$(1)/%,$(AVR_OBJS)) -lavr_$$(PART_$(1)) $(AVR_LFLAGS) -mmcu=$$(PART_$(1))
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

bld/obj/%.o:	%.cpp
	@mkdir -p `dirname $@`
	g++ $(CPP_CFLAGS) -c $< -o $@ -MMD

TRANSLATE_attiny84a:=t84
TRANSLATE_attiny85:=t85
TRANSLATE_atmega328p:=m328p
define translate_part
$(if $(TRANSLATE_$(1)),$(TRANSLATE_$(1)),$(1))
endef

%:	bld/avrbin/%.hex fuses_%
	avrdude -u -V -p $(call translate_part,$(PART_$@)) -b 115200 -B 1 $(AVR_PROG) -U flash:w:$<:i
	avr-size bld/avrbin/$@

fuses_8:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 200 $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xD9:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_tiny_8:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p t84 -b 115200 -B 200 $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xDD:m -U efuse:w:0x01:m

fuses_8_85:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p t85 -b 115200 -B 200 $(AVR_PROG) -U lfuse:w:0xD2:m -U hfuse:w:0xDD:m -U efuse:w:0xFF:m

fuses_16:
	# 16 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 200 $(AVR_PROG) -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_20:
	# 20 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 200 $(AVR_PROG) -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_12:
	# 12 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 200 $(AVR_PROG) -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

-include $(patsubst %.o,%.d,$(CPP_OBJS))
-include $(patsubst %.o,%.d,$(CPP_TEST_OBJS))
-include $(patsubst %.o,%.d,$(AVR_OBJS))
