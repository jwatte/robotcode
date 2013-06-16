
#include "USBLink.h"
#include "Settings.h"
#include "util.h"
#include <iostream>
#include <boost/lexical_cast.hpp>

#include <stdio.h>
#include "LUFA/OnyxWalker/MyProto.h"

unsigned char off_packet[] = {
    0,  //  serial
    OpSetStatus | 2,
    TargetPower,
    0
};

int main(int argc, char const *argv[]) {
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    boost::shared_ptr<Module> usbl(USBLink::open(st, boost::shared_ptr<Logger>()));
    USBLink *ul = usbl->cast_as<USBLink>();
    for (int i = 0; i < 3; ++i) {
        ul->raw_send(off_packet, sizeof(off_packet));
        ul->step();
        usleep(100000);
    }
    return 0;
}

