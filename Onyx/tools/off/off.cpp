#include "USBLink.h"
#include "Settings.h"
#include "util.h"
#include <iostream>
#include <boost/lexical_cast.hpp>


unsigned char off_packet[] = {
    0,      //  serial
    0x01, 0, // RAW mode, uart @ 2 Mbit
    0x00, // DATA
    0xff, 0xff, //  start packet
    0xfe,   //  id
    0x04,   //  length
    0x03,   //  set register
    0x18,   //  torque enable
    0x00,   //  off
    (unsigned char)~(0xfe + 0x04 + 0x03 + 0x18 + 0x00)   //  checksum
};

int main(int argc, char const *argv[]) {
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    boost::shared_ptr<Module> usbl(USBLink::open(st, boost::shared_ptr<Logger>()));
    USBLink *ul = usbl->cast_as<USBLink>();
    unsigned char init_junk = 0xfe;
    ul->raw_send(&init_junk, 1);
    for (int i = 0; i < 3; ++i) {
        ul->raw_send(off_packet, sizeof(off_packet));
        ul->step();
        usleep(100000);
    }
    return 0;
}

