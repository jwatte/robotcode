
SRCS:=$(wildcard mrpt/*/*.cpp)
OBJS:=$(patsubst mrpt/%.cpp,bld/obj/%.o,$(SRCS))
APPS:=$(patsubst mrpt/%/,%,$(sort $(dir $(SRCS))))

AVR_SRCS:=$(wildcard avr/*/*.cpp)
AVR_OBJS:=$(patsubst avr/%.cpp,bld/avrobj/%.o,$(AVR_SRCS))
AVR_BINS:=$(patsubst avr/%/,%,$(sort $(filter-out avr/libavr/,$(dir $(AVR_SRCS)))))
AVR_CFLAGS:=-Wall -Wno-switch -O3 -Os -mcall-prologues -mmcu=atmega328p -Iavr/libavr
AVR_LFLAGS:=-mmcu=atmega328p -Lbld/avrbin -lavr


MRPT_LIBS:=opengl base hwdrivers gui obs slam
MRPT_INCLUDES:=-I/usr/local/include/mrpt/mrpt-config -I/usr/local/include/mrpt/util \
  $(patsubst %,-I/usr/local/include/mrpt/%/include,$(MRPT_LIBS)) -I/usr/include/eigen3
MRPT_LFLAGS:=$(patsubst %,-lmrpt-%,$(MRPT_LIBS))
OPENCV_LFLAGS:=-lopencv_highgui -lopencv_core -lopencv_imgproc -lopencv_calib3d -lopencv_features2d -lopencv_video -lwebcam

all:	$(patsubst %,bld/bin/%,$(APPS)) $(patsubst %,bld/avrbin/%.hex,$(AVR_BINS))

clean:
	rm -rf bld
	mkdir bld

define mkapp
bld/bin/$(1):	$(filter bld/obj/$(1)/%,$(OBJS))
	mkdir -p `dirname $$@`
	g++ -o bld/bin/$(1) $(filter bld/obj/$(1)/%,$(OBJS)) \
		$(MRPT_LFLAGS) -lwebcam
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

bld/obj/%.o:	mrpt/%.cpp
	mkdir -p `dirname $@`
	g++ -g -O2 -c $< -o $@ -MMD -Ilib $(MRPT_INCLUDES)

bld/avrobj/%.o:	avr/%.cpp
	mkdir -p `dirname $@`
	avr-gcc $(AVR_CFLAGS) -c $< -o $@ -MMD -I/usr/lib/avr/include

%:	bld/avrbin/%.hex fuses
	avrdude -u -V -p m328p -b 115200 -B 0.5 -c usbtiny -U flash:w:$<:i

bld/avrbin/libavr.a:	$(filter bld/avrobj/libavr/%.o,$(AVR_OBJS))
	mkdir -p `dirname $@`
	ar cr $@ $^

fuses:
	# 8 MHz, internal osc, 2.7V brown-out, 65k + 4.1ms boot delay
	avrdude -e -u -V -p m328p -b 115200 -B 1 -c usbtiny -U lfuse:w:0xD2:m -U hfuse:w:0xD9:m -U efuse:w:0xFD:m -U lock:w:0xFF:m

-include $(patsubst %.o,%.d,$(OBJS))
-include $(patsubst %.o,%.d,$(AVR_OBJS))
