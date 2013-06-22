
#include "USBLink.h"
#include "Settings.h"
#include "util.h"
#include <iostream>
#include <boost/lexical_cast.hpp>

#include <stdio.h>
#include "LUFA/OnyxWalker/MyProto.h"

unsigned char set_packet[] = {
    0,  //  serial
    OpSetStatus | 2,
    TargetPower,
    0
};

unsigned char get_packet[] = {
    0,  //  serial
    OpGetStatus | 1,
    TargetPower
};

class StatusReceiver {

public:

    StatusReceiver() : state_(0), got_(false) {}

    bool decode(USBLink *link) {
        got_ = false;
again:
        size_t sz = 0;
        unsigned char const *buf = link->begin_receive(sz);
        size_t gsz = sz;
        if (sz > 0) {
            ++buf;
            --sz;
            unsigned char const *end = buf + sz;
            while (buf != end) {
                size_t u = decode(buf, end);
                buf += u;
            }
        }
        link->end_receive(gsz);
        if (gsz > 0) {
            goto again;
        }
        return got_;
    }

    unsigned char state_;

private:

    size_t decode(unsigned char const *ptr, unsigned char const *end) {
        unsigned char const *base = ptr;
        unsigned char sz = *ptr & 0xf;
        unsigned char cmd = *ptr & 0xf0;
        if (sz == 15) {
            if (ptr == end-1) {
                fprintf(stderr, "short packet\n");
                return end - base;
            }
            ++ptr;
            sz = *ptr;
        }
        ++ptr;
        if (ptr+sz > end) {
            fprintf(stderr, "short packet\n");
            return end - base;
        }
        if (cmd == OpGetStatus) {
            if (sz >= 4) {
                if (ptr[0] == TargetPower) {
                    state_ = ptr[3];
                    got_ = true;
                }
                else {
                    fprintf(stderr, "unknown target: 0x%x\n", ptr[0]);
                }
            }
            else {
                fprintf(stderr, "short status: %d\n", sz);
            }
        }
        else {
            fprintf(stderr, "unknown cmd: 0x%x\n", cmd);
        }
        return sz + ptr - base;
    }

    bool got_;
};

StatusReceiver sr;

struct powerbit {
    int bitval;
    char const *name;
};
powerbit powerbitvals[] = {
    { 1, "power" },
    { 2, "servos" },
    { 4, "fans" },
    { 8, "guns" },
};

char const *getbitname(int bitval) {
    for (size_t i = 0; i != sizeof(powerbitvals)/sizeof(powerbitvals[0]); ++i) {
        if (powerbitvals[i].bitval == bitval) {
            return powerbitvals[i].name;
        }
    }
    return 0;
}

int decodebit(char const *name) {
    for (size_t i = 0; i != sizeof(powerbitvals)/sizeof(powerbitvals[0]); ++i) {
        if (!strcmp(powerbitvals[i].name, name)) {
            return powerbitvals[i].bitval;
        }
    }
    return 0;
}


int main(int argc, char const *argv[]) {
    boost::shared_ptr<Settings> st(Settings::load("settings.ini"));
    boost::shared_ptr<Module> usbl(USBLink::open(st, boost::shared_ptr<Logger>()));
    USBLink *ul = usbl->cast_as<USBLink>();

    while (!sr.decode(ul)) {
        ul->raw_send(get_packet, sizeof(get_packet));
        ul->step();
        usleep(50000);
        ul->step();
    }
    unsigned char state = sr.state_;

    if (argc > 1) {
        for (int i = 1; i != argc; ++i) {
            unsigned char bit = 0;
            if (!strncmp(argv[i], "no", 2)) {
                bit = decodebit(argv[i] + 2);
                state &= ~bit;
            }
            else {
                bit = decodebit(argv[i]);
                state |= bit;
            }
            if (!bit) {
                fprintf(stderr, "unknown power bit name: %s\n", argv[i]);
                exit(1);
            }
        }
        set_packet[3] = state;
        while (sr.state_ != state && !sr.decode(ul)) {
            fprintf(stderr, "receive\n");
            ul->raw_send(set_packet, sizeof(set_packet));
            ul->raw_send(get_packet, sizeof(get_packet));
            ul->step();
            usleep(50000);
            ul->step();
        }
    }

    for (int i = 0; i != 8; ++i) {
        char const *bitname = getbitname(1 << i);
        if (bitname) {
            fprintf(stdout, (state & (1 << i)) ? "%s " : "no%s ", bitname);
        }
    }
    fprintf(stdout, "\n");

    return 0;
}

