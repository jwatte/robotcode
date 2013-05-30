
APP:=powerboard

PART:=attiny84a
PART_DUDE:=t84
AVR_PROG?=-c usbtiny
FUSES_B?=-B 10 -i 2
FLASH_B?=-B 1 -i 1
FUSE_VALS:=-U lfuse:w:0xd2:m -U hfuse:w:0xdd:m -U efuse:w:0x01:m

SRCS:=$(wildcard *.cpp)
OBJS:=$(patsubst %.cpp,bld/%.o,$(SRCS))

all:	bld/$(APP).hex

clean:
	rm -rf bld

flash:	bld/$(APP).hex
	avrdude -e -u -V -p $(PART_DUDE) -b 230400 $(FUSES_B) $(AVR_PROG) $(FUSE_VALS)
	avrdude -u -V -p $(PART_DUDE) -b 230400 $(FLASH_B) $(AVR_PROG) -U flash:w:$<:i
	avr-size bld/$(APP).elf

bld/$(APP).hex:	$(OBJS)

bld/%.o: %.cpp
	@mkdir -p bld
	avr-gcc -c -o $@ $< -mmcu=$(PART) -Os -ffunction-sections -fdata-sections
bld/%.hex:	bld/%.elf
	@mkdir -p bld
	rm -f $@
	avr-objcopy -R .eeprom -O ihex --set-section-flags=.eeprom=alloc,load --no-change-warnings --change-section-lma .eeprom=0 $< $@
	avr-objcopy -R .eeprom -O ihex $< $@
bld/%.elf:	$(OBJS)
	avr-gcc -o $@ $(OBJS) -mmcu=$(PART) -Wl,--gc-sections

.PRECIOUS: bld/%.elf
