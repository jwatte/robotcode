
#include "USBLink.h"
#include "Settings.h"
#include "util.h"
#include <iostream>
#include <boost/lexical_cast.hpp>

class R : public USBReceiver {
public:
    R() {}
    void on_data(unsigned char const *info, unsigned char sz) {
        std::cout << "data " << (int)sz << " bytes" << std::endl;
    }
};

unsigned char ping_packet[] = {
    0,      //  serial
    0x01, 0, // RAW mode, uart @ 2 Mbit
    0x00, // DATA
    0xff, 0xff, //  start packet
    0x1,   //  id
    0x02,   //  length
    0x01,   //  ping
    (unsigned char)~(1 + 0x02 + 0x01)   //  checksum
};

#define ID_OFFSET 6
#define CKSUM_OFFSET 9
void set_id(unsigned char id) {
    ping_packet[ID_OFFSET] = id;
    ping_packet[CKSUM_OFFSET] = 0xff - id - 2 - 1;
}

void do_packet(unsigned char const *start, size_t size) {
    unsigned char cksum = 0;
    for (size_t i = 0; i < size; ++i) {
        cksum += start[i];
    }
    if (cksum != 0xff) {
        std::cerr << "cksum error, got " << hexnum(start[size-1]) << " wanted "
            << hexnum(cksum - start[size-1]) << std::endl;
        return;
    }
    //  start[1] == len
    switch (start[2]) {
        case 0:
            std::cerr << "ping response from ";
            break;
        default:
            std::cerr << "unknown code " << start[2] << " from ";
            break;
    }
    std::cerr << hexnum(start[0]) << std::endl;
}

int main(int argc, char const *argv[]) {
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    boost::shared_ptr<Module> usbl(USBLink::open(st, boost::shared_ptr<Logger>()));
    USBLink *ul = usbl->cast_as<USBLink>();
    unsigned char init_junk = 0xfe;
    ul->raw_send(&init_junk, 1);
    time_t last = 0;
    if (argc > 1) {
        set_id(boost::lexical_cast<unsigned int>(argv[1]));
        argc--;
        argv++;
    }
    while (true) {
        time_t now;
        time(&now);
        if (now != last) {
            std::cerr << "ping " << hexnum(ping_packet[ID_OFFSET]) << " @ " << now << std::endl;
            ul->raw_send(ping_packet, sizeof(ping_packet));
            last = now;
        }
        ul->step();
        size_t sz = 0;
        unsigned char const *d = ul->begin_receive(sz);
        size_t o = 0;
        for (o = 0; o < sz; ++o) {
            if (d[o] == 0xff) {
                break;
            }
        }
        if (sz - o < 6) {
            ul->end_receive(o);
            //  wait for more of packet
        }
        else if (d[o + 1] != 0xff) {
            ul->end_receive(o+1);
        }
        else if (d[o + 4] + 6u > sz - o) {
            ul->end_receive(o);
        }
        else if (sz > 0) {
            do_packet(d + o + 2, d[o + 4] + 4);
            ul->end_receive(o + 6 + d[o + 4]);
            if (argc > 1) {
                set_id(boost::lexical_cast<unsigned int>(argv[1]));
                argc--;
                argv++;
                last = 0;
            }
            else {
                break;
            }
        }
    }
    return 0;
}

