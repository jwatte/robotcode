
CPPS:=$(wildcard *.cpp)
APPS:=$(patsubst %.cpp,%,$(CPPS))

all:	$(APPS)

%:	%.cpp
	g++ -o $@ $< -I /usr/include/libusb-1.0 -O0 -g -pipe -std=gnu++11 -lusb-1.0

clean:
	rm -f $(APPS) *.o
