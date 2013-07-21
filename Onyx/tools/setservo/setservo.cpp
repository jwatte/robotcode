
#include "ServoSet.h"
#include "USBLink.h"
#include <math.h>
#include <boost/lexical_cast.hpp>

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <sys/time.h>
#include <unistd.h>
#include <map>
#include <sstream>


void usage() {
    fprintf(stderr, "usage: setservo [-v] [-t torque] id:position ...\n");
    fprintf(stderr, "id       = 1 .. 254\n");
    fprintf(stderr, "position = 0 .. 4095\n");
    fprintf(stderr, "torque   = 0 .. 1023\n");
    exit(1);
}

std::map<int, int> positions;

class L : public Logger {
public:
    L() {}
    virtual void log_data(LogWhat what, void const *data, size_t size)
    {
        std::stringstream ss;
        ss << std::hex << (int)what;
        for (size_t i = 0; i < size; ++i)
        {
            ss << " " << (int)((unsigned char *)data)[i];
        } std::cerr << ss.str() << std::endl;
    }
};

int main(int argc, char const *argv[]) {
    int torque = 700;
    boost::shared_ptr<Logger> logger;
again:
    if (argv[1] && !strcmp(argv[1], "-v")) {
        logger.reset(new L());
        --argc;
        ++argv;
        goto again;
    }
    else if (argv[1] && !strcmp(argv[1], "-t")) {
        if (!argv[2]) {
            usage();
        }
        torque = atoi(argv[2]);
        if (torque < 1 || torque > 1023) {
            usage();
        }
        argc -= 2;
        argv += 2;
        goto again;
    }
    if (argc < 2) {
        usage();
    }
    ServoSet ss(true, logger);
    for (int i = 1; i < argc; ++i) {
        int id, pos;
        if (sscanf(argv[i], "%d:%d", &id, &pos) != 2) {
            usage();
        }
        if (id < 1 || id > 254 || pos < 0 || pos > 4095) {
            usage();
        }
        positions[id] = pos;
    }
    for (auto ptr(positions.begin()), end(positions.end()); ptr != end; ++ptr) {
        ss.add_servo((*ptr).first, (*ptr).second);
    }
    ss.set_power(7);
    int i = 0;
    int nst = 0;
    while (true) {
        ss.step();
        usleep(50000);
        ++i;
        if (i == 10) {
            ss.set_torque(torque);
            for (auto ptr(positions.begin()), end(positions.end()); ptr != end; ++ptr) {
                ss.id((*ptr).first).set_goal_position((*ptr).second);
            }
        }
        if (!(i & 31)) {
            fprintf(stderr, "battery: %.1f\n", ss.battery() / 100.0f);
            unsigned char sliced[32] = { 0 };
            unsigned char n = ss.slice_reg1(REG_PRESENT_TEMPERATURE, sliced, 32);
            for (unsigned char j = 0; j != n; ++j) {
                std::cerr << " " << (int)sliced[j] << "C";
            }
            std::cerr << std::endl;
        }
        if (!nst) {
            unsigned char bb[32] = { 0 };
            int st = ss.get_status(bb, 32);
            unsigned char mask = 0;
            for (int i = 0; i < st; ++i) {
                mask |= bb[i];
            }
            if (mask != 0) {
                fprintf(stderr, "status=0x%02x\n", mask);
                for (int i = 0; i < st; ++i) {
                    fprintf(stderr, " %02x", bb[i]);
                }
                fprintf(stderr, "\n");
                nst = 10;
            }
        }
        else {
            --nst;
        }
    }
    return 0;
}

