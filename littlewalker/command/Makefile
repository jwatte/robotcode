
APP:=command
OBJ_DIR:=../../bld/obj/$(APP)
BIN_DIR:=../../bld/bin
APP_CPPS:=$(wildcard *.cpp)
APP_OS:=$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(APP_CPPS))
APP_DS:=$(patsubst %.o,%.d,$(APP_OS))
APP_BIN:=$(BIN_DIR)/$(APP)

all:	$(APP_BIN)

$(APP_BIN):	$(APP_OS)
	@mkdir -p $(BIN_DIR)
	g++ -o $@ $(APP_OS) -g -lusb-1.0 -lrt -lcrypto $(shell fltk-config --use-images --ldflags)

$(OBJ_DIR)/%.o:	%.cpp
	@mkdir -p $(OBJ_DIR)
	g++ -Wall -c -std=gnu++11 -I /usr/include/libusb-1.0 -O0 -g -pipe $(shell fltk-config --use-images --cxxflags) -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(APP_BIN)

-include $(APP_DS)
