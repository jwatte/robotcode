
APP:=control
OBJ_DIR:=../../bld/obj/$(APP)
BIN_DIR:=../../bld/bin
APP_CPPS:=$(wildcard *.cpp)
LIB_CPPS:=$(wildcard ../lib/*.cpp)
APP_OS:=$(patsubst %.cpp,$(OBJ_DIR)/%.o,$(APP_CPPS)) $(patsubst ../lib/%.cpp,$(OBJ_DIR)/%.o,$(LIB_CPPS))
APP_DS:=$(patsubst %.o,%.d,$(APP_OS))
APP_BIN:=$(BIN_DIR)/$(APP)

all:	$(APP_BIN)

$(APP_BIN):	$(APP_OS)
	@mkdir -p $(BIN_DIR)
	g++ -o $@ $(APP_OS) -g -lusb-1.0 -lrt -lcrypto -lv4l2

$(OBJ_DIR)/%.o:	../lib/%.cpp
	@mkdir -p $(OBJ_DIR)
	g++ -Wall -c -std=gnu++11 -I /usr/include/libusb-1.0 -O0 -g -pipe -MMD -o $@ $<

$(OBJ_DIR)/%.o:	%.cpp
	@mkdir -p $(OBJ_DIR)
	g++ -Wall -c -std=gnu++11 -I /usr/include/libusb-1.0 -O0 -g -pipe -MMD -o $@ $<

clean:
	rm -rf $(OBJ_DIR) $(APP_BIN)

-include $(APP_DS)
