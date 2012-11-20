
CPPS:=$(wildcard *.cpp)
APPS:=$(patsubst %.cpp,../../bld/bin/%,$(CPPS))

all:	$(APPS)

../../bld/bin/%:	%.cpp
	@mkdir -p $(dir $@)
	g++ -o $@ $< -MMD -I /usr/include/libusb-1.0 -O0 -g -pipe -std=gnu++11 -lusb-1.0

clean:
	rm -f $(APPS) *.o

-include $(patsubst %,%.d,$(APPS))
