
APPS:=robolink

CPP_CFLAGS:=$(sort -O0 -g $(filter-out -O%,$(shell fltk-config --use-images --cxxflags)))
CPP_LFLAGS:=$(sort $(shell fltk-config --use-images --ldflags))
CPP_SRCS:=$(foreach app,$(APPS),$(wildcard $(app)/*.cpp))
CPP_OBJS:=$(patsubst %.cpp,bld/obj/%.o,$(CPP_SRCS))

AVR_SRCS:=$(wildcard avr/*/*.cpp)
AVR_OBJS:=$(patsubst avr/%.cpp,bld/avrobj/%.o,$(AVR_SRCS))
AVR_BINS:=$(patsubst avr/%/,%,$(sort $(filter-out avr/libavr/,$(dir $(AVR_SRCS)))))
AVR_CFLAGS:=-Wall -Wno-switch -Os -mcall-prologues -mmcu=atmega328p -Iavr/libavr -std=gnu++0x
AVR_LFLAGS:=-mmcu=atmega328p -Lbld/avrbin -lavr -lc


OPENCV_LFLAGS:=-lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_calib3d -lopencv_features2d -lopencv_video -lwebcam

all:	$(patsubst %,bld/bin/%,$(APPS)) $(patsubst %,bld/avrbin/%.hex,$(AVR_BINS))

clean:
	rm -rf bld
	mkdir bld

define mkapp
bld/bin/$(1):	$(filter bld/obj/$(1)/%,$(CPP_OBJS))
	@mkdir -p `dirname $$@`
	g++ -o bld/bin/$(1) $(filter bld/obj/$(1)/%,$(CPP_OBJS)) $(CPP_LFLAGS)
endef
$(foreach app,$(APPS),$(eval $(call mkapp,$(app))))

define mkavrbin
bld/avrbin/$(1):	$(filter bld/avrobj/$(1)/%,$(AVR_OBJS)) bld/avrbin/libavr.a
	mkdir -p `dirname $$@`
	avr-gcc -o bld/avrbin/$(1) $(filter bld/avrobj/$(1)/%,$(AVR_OBJS)) $(AVR_LFLAGS)
bld/avrbin/$(1).hex:	bld/avrbin/$(1)
	rm -f $$@
	mkdir -p `dirname $$@`
	avr-objcopy -R .eeprom -O ihex --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 $$< $$@
	avr-objcopy -R .eeprom -O ihex $$< $$@
endef
$(foreach avrbin,$(AVR_BINS),$(eval $(call mkavrbin,$(avrbin))))

define filter_avr_cflags
$(if $(findstring DefaultFonts,$(1)),$(filter-out -O3,$(2)),$(2))
endef

bld/avrobj/%.o:	avr/%.cpp
	@mkdir -p `dirname $@`
	avr-gcc $(call filter_avr_cflags,$<,$(AVR_CFLAGS)) -c $< -o $@ -MMD -I/usr/lib/avr/include

bld/obj/%.o:	%.cpp
	@mkdir -p `dirname $@`
	g++ $(CPP_CFLAGS) -c $< -o $@ -MMD

fuses_motorboard:	fuses_8
fuses_estop:	fuses_8
fuses_usbboard:	fuses_16
fuses_sensorboard:	fuses_8

%:	bld/avrbin/%.hex fuses_%
	avrdude -u -V -p m328p -b 115200 -B 1 -c usbtiny -U flash:w:$<:i

bld/avrbin/libavr.a:	$(filter bld/avrobj/libavr/%.o,$(AVR_OBJS))
	@mkdir -p `dirname $@`
	ar cr $@ $^

fuses_8:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 1000 -c usbtiny -U lfuse:w:0xD2:m -U hfuse:w:0xD9:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

fuses_16:
	# 16 MHz, crystal osc, 2.7V brown-out, 16k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 1000 -c usbtiny -U lfuse:w:0xE7:m -U hfuse:w:0xDF:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

-include $(patsubst %.o,%.d,$(OBJS))
-include $(patsubst %.o,%.d,$(AVR_OBJS))
